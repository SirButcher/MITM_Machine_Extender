#pragma once

// --------------- OUTGOING LINES ---------------

// Pin used to send bit0 - IRQ line toward the master.
const int pin_IRQToMaster = 6;

// Pin used to send bit1 - Binary data
const int pin_binaryDataOut = 7;

// Enable the isolator used for communication
const int isolatorEnable = 8;

// Led used to display the active communication
const int CommLed = 10;

// --------------- INCOMING LINES ---------------

// Pin used to read bit0 - Data Request signal
const int pin_dataRequest = 3;

// Pin used to read bit1 - Clock signal
const int pin_MasterClock = 4;

// IRQ line from the Keyboard.
const int pin_Keyboard_IRQ = 2;