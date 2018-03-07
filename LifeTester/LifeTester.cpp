#include "Arduino.h"
#include "Config.h"
#include "IoWrapper.h"
#include "IV.h"
#include "I2C.h"
#include "LedFlash.h"
#include "LifeTesterTypes.h"
#include "MCP4802.h" // dac types
#include "Print.h"
#include <SPI.h>
#include <Wire.h>

//////////////////////////////////
//Initialise lifetester channels//
////////////////////////////////// 
LifeTester_t channelA = {
  {chASelect, 0U},    // io
  Flasher(LED_A_PIN), // led
  {0},                // data
  NULL,               // state function
  0U,                 // timer
  ok                  // error
};

LifeTester_t channelB = {
  {chBSelect, 1U},
  Flasher(LED_B_PIN),
  {0},
  NULL,
  0U,
  ok
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
  
  //MPP INITIAL SEARCH/SCAN
  Serial.println("Scanning for MPP...");
  // Scan IV and initialise DACs to MPP initial guess
  IV_ScanAndUpdate(&channelA, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN);
  IV_ScanAndUpdate(&channelB, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN); 

  //DATA HEADINGS
  Serial.println();
  Serial.println("Tracking max power point...");
  Serial.println("DACx, ADCx, power, error, Light Sensor, T(C), channel");

  //SETUP LEDS FOR MAIN LOOP
  channelA.led.t(50, 50);
  channelB.led.t(50, 50);

  //set timer ready for measurements
  channelA.timer = channelB.timer = millis();

  Serial.println("Finished setup. Entering main loop.");
}

void loop()
{
  IV_MpptUpdate(&channelA);
  IV_MpptUpdate(&channelB);

  channelA.led.update();
  channelB.led.update();

  TempSenseUpdate();
  I2C_PrepareData(&channelA, &channelB);
}