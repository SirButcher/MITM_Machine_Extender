/*
	Name:       MITM Keyboard Processor.ino
	Created:	10/05/2019 11:12:29 AM
	Author:     Gabriel Butcher
*/

// This application is designed to be able to read the Presto's keyboard
// and be able to submit the pressed data toward the Processing Unit.

// Each data segment will be built in the following way:
// Byte 0: Confirmation byte - value must be set to 1 to confirm the connection
// Byte 1: Group data - which group is pressed
// Byte 2: Key data - which pin is pressed.

#include "PinLayout.h"

#include "mpr121.h"
#include <Wire.h>

const int addresses[] = { 0x5A, 0x5B, 0x5C, 0x5D };

// Maximum allowed time durin data sending, in uS
const int dataSendingTimeOut = 5000000;

static bool touchStates[12 * 4]; //to keep track of the previous touch states
static bool isPadTouched = false;

static byte touched_address = 0;
static byte touched_pinByte0 = 0;
static byte touched_pinByte1 = 0;


static byte _responseArray[4] = { 1, 0, 0, 0 };


// The setup() function runs once each time the micro-controller starts
void setup()
{
	pinMode(pin_Keyboard_IRQ, INPUT);

	// Setup our pins:
	pinMode(pin_Keyboard_IRQ, INPUT);

	pinMode(pin_dataRequest, INPUT);
	pinMode(pin_MasterClock, INPUT);

	pinMode(pin_IRQToMaster, OUTPUT);
	digitalWrite(pin_IRQToMaster, HIGH);	// Active LOW

	pinMode(pin_binaryDataOut, OUTPUT);
	digitalWrite(pin_binaryDataOut, LOW);

	pinMode(isolatorEnable, OUTPUT);
	digitalWrite(isolatorEnable, HIGH);		// Enable the isolator circuit.

	pinMode(CommLed, OUTPUT);
	digitalWrite(CommLed, LOW);


	Wire.begin();

	SetupKeyboard(0x5A);
	SetupKeyboard(0x5B);
	SetupKeyboard(0x5C);
	SetupKeyboard(0x5D);


	// Keyboard setup is completed.
}

// Add the main program code into the continuous loop() function
void loop()
{
	// Okay, loopy-time!
	// We need to check the keyboard first, then check if the Master device
	// requires us to send data.
	if (readTouchInputs())
	{
		// Data is read AND changed. We should store them in the array, in case we need it to be sent.
		_responseArray[1] = touched_address;
		_responseArray[2] = touched_pinByte0;
		_responseArray[3] = touched_pinByte1;

		Serial.print("Address: ");
		Serial.print(touched_address);
		Serial.print(" - Byte0: ");
		Serial.print(touched_pinByte0);
		Serial.print(" - Byte1: ");
		Serial.println(touched_pinByte1);

		// Notify the master about this new situation. - Active LOW
		digitalWrite(pin_IRQToMaster, LOW);
	}

	// Now we should check if the master want to read this data or not.
	if (digitalRead(pin_dataRequest))
	{
		// Blink the Comm led
		digitalWrite(CommLed, HIGH);

		// We should send the data to the master.
		SendMassDataToMaster(_responseArray, 4, pin_MasterClock, pin_binaryDataOut);

		// Blink the Comm led
		digitalWrite(CommLed, LOW);

		// Data is sent! Set the IWQToMaster to inactive.
		digitalWrite(pin_IRQToMaster, HIGH);
	}

	// Delay a little so we won't warm up.
	delayMicroseconds(10);
	//delay(50);
}

//--------------------------------------

void SendDataToMaster(byte data, int clockPin, int dataPin)
{
	int i;
	uint8_t dataToSend;

	for (i = 7; i >= 0; i--)
	{
		// Read the next bit
		dataToSend = ((data >> i) & 0x01);

		// Wait till the clock signal switch to HIGH
		if (!waitTillSignal(clockPin, 1))
			return; // Timed out!!!

		// Okay, data is sent!
		digitalWrite(dataPin, dataToSend);

		//Now wait till we get a confirmation - by switching the clock to LOW.
		if (!waitTillSignal(clockPin, 0))
			return; // Timed out!!!

		// Now repeat and wait till the master is ready to receive the next bit.
	}
}

// Wait till the pin is reach the expected value. Returns false if timed out!
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
		if (elapsedTime > dataSendingTimeOut)
		{
			return false;
		}
	}

	return true;
}

void SendMassDataToMaster(byte arr[], int size, int clockPin, int dataPin)
{
	int i;

	for (i = 0; i < size; i++)
	{
		SendDataToMaster(arr[i], clockPin, dataPin);
	}
}

//--------------------------------------

bool readTouchInputs()
{
	int address;
	int group;

	bool stateChanged = false;

	if (HasKeyboardData())
	{
		// We detected an IRQ signal - which means the keyboard is wish to send data.
		for (group = 0; group < 4; group++)
		{
			// Scan all address to find which IC want to send us data

			address = addresses[group];

			//read the touch state from the MPR121
			Wire.requestFrom(address, 2);

			byte LSB = Wire.read();
			byte MSB = Wire.read();

			uint16_t touched = ((MSB << 8) | LSB); //16bits that make up the touch states

			for (int i = 0; i < 12; i++)
			{  // Check what electrodes were pressed

				if (touched & (1 << i))
				{
					// Only care about the touch event if no pad is touched.
					if (!isPadTouched)
					{
						if (touchStates[i + (group * 12)] == 0)
						{
							//pin i was just touched
							isPadTouched = true;
							stateChanged = true;

							// Store the touched pin. 0 will be the "No Pin Touched"
							// So we offset everything with 1.
							touched_address = address;
							touched_pinByte0 = LSB;
							touched_pinByte1 = MSB;
						}
						else if (touchStates[i + (group * 12)] == 1)
						{
							//pin i is still being touched
						}

						touchStates[i + (group * 12)] = 1;
					}
				}
				else
				{
					if (touchStates[i + (group * 12)] == 1)
					{
						//pin i is no longer being touched
						isPadTouched = false;
						stateChanged = true;

						touched_address = 0;
						touched_pinByte0 = 0;
						touched_pinByte1 = 0;
					}

					touchStates[i + (group * 12)] = 0;
				}
			}

		}


		return stateChanged;
	}
}

bool HasKeyboardData(void) {

	// The keyboard will signal us with a LOW value if there is data available
	return !digitalRead(pin_Keyboard_IRQ);
}

void mpr121_setup(int address) {

	set_register(address, ELE_CFG, 0x00);

	// Section A - Controls filtering when data is > baseline.
	set_register(address, MHD_R, 0x01);
	set_register(address, NHD_R, 0x01);
	set_register(address, NCL_R, 0x00);
	set_register(address, FDL_R, 0x00);

	// Section B - Controls filtering when data is < baseline.
	set_register(address, MHD_F, 0x01);
	set_register(address, NHD_F, 0x01);
	set_register(address, NCL_F, 0xFF);
	set_register(address, FDL_F, 0x02);

	// Section C - Sets touch and release thresholds for each electrode
	set_register(address, ELE0_T, TOU_THRESH);
	set_register(address, ELE0_R, REL_THRESH);

	set_register(address, ELE1_T, TOU_THRESH);
	set_register(address, ELE1_R, REL_THRESH);

	set_register(address, ELE2_T, TOU_THRESH);
	set_register(address, ELE2_R, REL_THRESH);

	set_register(address, ELE3_T, TOU_THRESH);
	set_register(address, ELE3_R, REL_THRESH);

	set_register(address, ELE4_T, TOU_THRESH);
	set_register(address, ELE4_R, REL_THRESH);

	set_register(address, ELE5_T, TOU_THRESH);
	set_register(address, ELE5_R, REL_THRESH);

	set_register(address, ELE6_T, TOU_THRESH);
	set_register(address, ELE6_R, REL_THRESH);

	set_register(address, ELE7_T, TOU_THRESH);
	set_register(address, ELE7_R, REL_THRESH);

	set_register(address, ELE8_T, TOU_THRESH);
	set_register(address, ELE8_R, REL_THRESH);

	set_register(address, ELE9_T, TOU_THRESH);
	set_register(address, ELE9_R, REL_THRESH);

	set_register(address, ELE10_T, TOU_THRESH);
	set_register(address, ELE10_R, REL_THRESH);

	set_register(address, ELE11_T, TOU_THRESH);
	set_register(address, ELE11_R, REL_THRESH);

	// Section D
	// Set the Filter Configuration
	// Set ESI2
	set_register(address, FIL_CFG, 0x04);

	// Section E
	// Electrode Configuration
	// Set ELE_CFG to 0x00 to return to standby mode
	set_register(address, ELE_CFG, 0x0C);  // Enables all 12 Electrodes


	// Section F
	// Enable Auto Config and auto Reconfig
	// set_register(address, ATO_CFG0, 0x0B);
	// set_register(address, ATO_CFGU, 0xC9);  // USL = (Vdd-0.7)/vdd*256 = 0xC9 @3.3V   
	// set_register(address, ATO_CFGL, 0x82);  // LSL = 0.65*USL = 0x82 @3.3V
	// set_register(address, ATO_CFGT, 0xB5);  // Target = 0.9*USL = 0xB5 @3.3V

	set_register(address, ELE_CFG, 0x0C);
}


void SetupKeyboard(int address) {

	set_register(address, 43, 16);
	set_register(address, 44, 8);
	set_register(address, 45, 3);
	set_register(address, 46, 4);
	set_register(address, 47, 12);
	set_register(address, 48, 4);
	set_register(address, 49, 3);
	set_register(address, 50, 4);
	set_register(address, 125, 3);
	set_register(address, 91, 17);
	set_register(address, 65, 30);
	set_register(address, 66, 24);
	set_register(address, 67, 30);
	set_register(address, 68, 24);
	set_register(address, 69, 30);
	set_register(address, 70, 24);
	set_register(address, 71, 30);
	set_register(address, 72, 24);
	set_register(address, 73, 30);
	set_register(address, 74, 24);
	set_register(address, 75, 30);
	set_register(address, 76, 24);
	set_register(address, 77, 30);
	set_register(address, 78, 24);
	set_register(address, 79, 30);
	set_register(address, 80, 24);
	set_register(address, 81, 30);
	set_register(address, 18, 24);
	set_register(address, 83, 30);
	set_register(address, 84, 24);
	set_register(address, 85, 30);
	set_register(address, 86, 24);
	set_register(address, 87, 30);
	set_register(address, 88, 24);
	set_register(address, 125, 156);
	set_register(address, 126, 101);
	set_register(address, 127, 140);
	set_register(address, 123, 11);
	set_register(address, 94, 140);
	set_register(address, 0, 1);
	set_register(address, 1, 2);
	set_register(address, 3, 94);
	set_register(address, 94, 0);
	set_register(address, 32, 64);
	set_register(address, 96, 128);
	set_register(address, 99, 94);
}

void set_register(int address, unsigned char r, unsigned char v) {
	Wire.beginTransmission(address);
	Wire.write(r);
	Wire.write(v);
	Wire.endTransmission();
}