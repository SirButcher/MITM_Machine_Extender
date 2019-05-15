#pragma once


// --------------- OUTGOING LINES ---------------

// Enable the isolator used for communication
const int isolatorEnable = 8;

// Led used to display the active communication
const int CommLed = 10;

// IRQ line toward the Presto Machine
const int pin_Keyboard_IRQ = 6;

// --------------- INCOMING LINES ---------------

// Pin used to read bit0 - Master data available and clock signal
const int pin_IRQ_Clock = 7;

// Pin used to read bit1 - Master binary data in
const int pin_BinaryData = 8;

// -------------- Global Variables --------------

// I2C addresses
const char addresses[4] = { 0x5A, 0x5B, 0x5C };

// Available Address count
const int addressCount = 4;

// Buffer with two zeroes.
const byte emptyBuffer[] = { 0, 0 };
