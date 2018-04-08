#include "Arduino.h"
#include "Config.h"
#include "IoWrapper.h"
#include "StateMachine.h"
#include "I2C.h"
#include "LedFlash.h"
#include "LifeTesterTypes.h"
#include "MCP4802.h" // dac types
#include "Print.h"
#include <SPI.h>
#include <Wire.h>

// Lifetester channel data
LifeTester_t channelA = {
  {chASelect, 0U},    // io
  Flasher(LED_A_PIN), // led
  {0},                // data
  0U,                 // timer
  ok,                 // error
  NULL                // state
};

LifeTester_t channelB = {
  {chBSelect, 1U},
  Flasher(LED_B_PIN),
  {0},
  0U,
  ok,
  NULL,
};

void setup()
{ 
  // SERIAL PORT COMMUNICATION WITH PC VIA UART
  Serial.begin(9600);
  SPI.begin();
    
  // I2C COMMUNICATION WITH MASTER ARDUINO
  Wire.begin(I2C_ADDRESS);          // I2C address defined at compile time - see makefile
  Wire.onRequest(I2C_TransmitData); // register event

  // INITIALISE I/O
  Serial.println("Initialising IO...");
  DacInit();
  AdcInit();
  TempSenseInit();
  
  StateMachine_Reset(&channelA);
  StateMachine_Reset(&channelB);

  Serial.println("Finished setup. Entering main loop.");
}

void loop()
{
  StateMachine_UpdateStep(&channelA);
  StateMachine_UpdateStep(&channelB);

  TempSenseUpdate();
  I2C_PrepareData(&channelA, &channelB);
}