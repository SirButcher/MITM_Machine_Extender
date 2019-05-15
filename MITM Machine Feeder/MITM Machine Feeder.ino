/*
 Name:		MITM_Machine_Feeder.ino
 Created:	14/05/2019 03:04:21 PM
 Author:	Gabriel Butcher
*/

// This application is designed to be able to feed data to the Presto's central processing unit.

// To achieve this, we will impersonate the keyboard, and act like we are the keyboard.
// The keyboard has 4 IC, with four different I2C address - we need to respond to each one of them.
// The keyboard use the IRQ line to check if there is any data. If we want to submit data
// we need to set this to LOW - after this, the machine will start to query the addresses.

// The master will send us data, 4 bytes at a time, to submit to the machine.
// The master will notify us about the available data by setting the IRQ/Clock line to LOW.
// After that, the IRQ/Clock line will act as a regular Clock line.

// Each data segment will be built in the following way:
// Byte 0: Confirmation byte - value must be set to 1 to confirm the connection
// Byte 1: Group data - address of the group what user pressed.
// Byte 2: LSB - first byte to send to the machine
// Byte 2: MSB - second byte to send to the machine

#include "TwoWireSimulator.h"
#include "PinLayout.h"

TwoWireSimulator WireSim;

char addressToRespond;
byte dataToSend[] = { 0, 0 };

byte masterData[] = { 0, 0, 0, 0 };

// Maximum allowed time durin data sending, in uS
const int dataReadingTimeOut = 5000000;

// the setup function runs once when you press reset or power the board
void setup() {

	Serial.begin(9600);
	Serial.println("MITM Machine feeder starting!");


	WireSim.begin(addresses[0], 15);

	// Register for the events:
	WireSim.onReceive(receiveEvent);
	WireSim.onRequest(requestEvent);


	// Set up the input / output lines
	pinMode(isolatorEnable, OUTPUT);
	digitalWrite(isolatorEnable, HIGH);

	pinMode(CommLed, OUTPUT);
	digitalWrite(isolatorEnable, HIGH);

	pinMode(pin_Keyboard_IRQ, OUTPUT);
	digitalWrite(isolatorEnable, HIGH);

	pinMode(pin_IRQ_Clock, INPUT);
	pinMode(pin_BinaryData, INPUT);

	Serial.println("Setup done!");
}

// the loop function runs over and over again until power down or reset
void loop() {

	// Check if the master want to send us some data:
	if (digitalRead(pin_IRQ_Clock))
	{
		Serial.println("IRQ detected!");

		// Reset the master data (so we can check the comm's status)
		masterData[0] = 0;

		// Blink the Comm led
		// digitalWrite(CommLed, HIGH);

		bool dataTransmitSuccess = false;

		// Okay, the master want to send us some data!
		if (ReadMassDataFromMaster(masterData, 4, pin_IRQ_Clock, pin_BinaryData))
		{
			// The process didn't timed out - but this doesn't mean it was succesful!
			// the first byte must be exactly 1 or it is invalid.
			if (masterData[0] == 1)
			{
				// Everything is OK!
				dataTransmitSuccess = true;

				// First, save the address:
				addressToRespond = masterData[1];

				// Then the bytes what we want to send:
				dataToSend[0] = masterData[2];
				dataToSend[1] = masterData[3];

				Serial.print("Arrived data: ");
				Serial.print(addressToRespond, DEC);
				Serial.print(" - ");

				Serial.print(dataToSend[0], DEC);
				Serial.print(" - ");

				Serial.println(dataToSend[1], DEC);


				// Signal the machine about the new data:
				digitalWrite(pin_Keyboard_IRQ, LOW);

				Serial.println("Data transfer was succesful!");
			}
			else
			{
				Serial.print("Data arrived but the checkbyte is invalid.");
			}
		}
		

		// Check if we was succesful or not
		if(!dataTransmitSuccess)
		{
			Serial.println("Data transfer failed!");

			// Clear everything, just to be sure.
			addressToRespond = 0;
			dataToSend[0] = 0;
			dataToSend[1] = 0;

			// No data is there, nothing to see, disperse, please.
			digitalWrite(pin_Keyboard_IRQ, HIGH);
		}

		// Blink the Comm led
		// digitalWrite(CommLed, LOW);
	}

	delayMicroseconds(100);
}

bool ReadMassDataFromMaster(byte array[], int byteToRead, int clockPin, int dataPin)
{
	// This method will read, and store the read data into the array.
	Serial.println("Starting byte reading.");

	// Set a bit in a byte:
	// vale |= [SetTo] << [Where];
	// Example: testData |= 1 << 7; - setting the 8th bit to 1


	// First, wait till the pin_IRQ_Clock switch to LOW - this will signal that the master is getting ready to transmit data.
	if (!waitTillSignal(clockPin, 0))
		return false; // Timed out!!!

	Serial.println("Switching to clock mode.");

	int i, b;
	for (i = 0; i < byteToRead; i++)
	{
		// The master is ready to send us the data.
		// We have to wait until the Clock is high
		// then read a bit, wait till the clock is LOW
		// and so on.

		for (b = 7; b >= 0; b--)
		{
			// Wait till the clock signal switch to HIGH
			if (!waitTillSignal(clockPin, 1))
				return false; // Timed out!!!

			// Read the bit
			int bit = digitalRead(dataPin);


			// Set the bit:
			if(bit == 1)
				array[i] |= 1 << b;
			else
				array[i] &= ~(1 << b);

			// Now wait till the signal goes LOW.
			if (!waitTillSignal(clockPin, 0))
				return false; // Timed out!!!
		}
	}

	return true;
}

boolean waitTillSignal(int pin, int expectedValue)
{
	int delayInUs = 5;
	int elapsedTime = 0;

	while (digitalRead(pin) != expectedValue)
	{
		delayMicroseconds(delayInUs);
		elapsedTime += delayInUs;

		// Check if we waited too much.
		// If we over than the allowed time, then skip
		// so the app won't be frozen forever.
		if (elapsedTime > dataReadingTimeOut)
		{
			Serial.println("Timed out!");
			return false;
		}
	}

	return true;
}

// ---------------- I2C Events ----------------

// Called when Master wants value from slave
void requestEvent() {

	// Blink the Comm led
	// digitalWrite(CommLed, HIGH);

	// Check which address is being checked:
	char address = WireSim.lastAddress();

	// Check if the data is 
	if (addressToRespond == address)
	{
		// Blink the Comm led
		digitalWrite(CommLed, HIGH);

		WireSim.write(dataToSend, 2);

		// We should reset the IRQLine to the default (HIGH) value as
		// the machine downloaded the data.
		digitalWrite(pin_Keyboard_IRQ, HIGH);
	}
	else
	{
		// This isn't the address where the data is!
		// Respond with the empty buffer, so the machine can keep looking.
		WireSim.write(emptyBuffer, 2);
	}

	// Blink the Comm led
	//digitalWrite(CommLed, LOW);

}

// Called when Slave receives value from master
void receiveEvent(int arrivedByteCount) {

	int i;
	for (i = 0; i < arrivedByteCount; i++)
	{
		byte arrivedData = Wire.read();

		// We don't really care about the incoming data, as
		// the machine only sending the config, and we already know it
		// But we should read it in case it cause some strange error.
	}
}
