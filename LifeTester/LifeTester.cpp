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
LifeTester_t LTChannelA = {
  {chASelect, 0U},  // dac, adc - TODO: check that these are correct settings!
  Flasher(LED_A_PIN),
  0,
  0,
  0,
  ok,
  {0},
  0
};

LifeTester_t LTChannelB = {
  {chBSelect, 1U},  // dac, adc
  Flasher(LED_B_PIN),
  0,
  0,
  0,
  ok,
  {0},
  0
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
  IV_ScanAndUpdate(&LTChannelA, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN);
  IV_ScanAndUpdate(&LTChannelB, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN); 

  //DATA HEADINGS
  Serial.println();
  Serial.println("Tracking max power point...");
  Serial.println("DACx, ADCx, power, error, Light Sensor, T(C), channel");

  //SETUP LEDS FOR MAIN LOOP
  LTChannelA.Led.t(50, 50);
  LTChannelB.Led.t(50, 50);

  //set timer ready for measurements
  LTChannelA.timer = LTChannelB.timer = millis();

  Serial.println("Finished setup. Entering main loop.");
}

void loop()
{
  IV_MpptUpdate(&LTChannelA);
  IV_MpptUpdate(&LTChannelB);

  //LED will update every time the loop runs
  LTChannelA.Led.update();
  LTChannelB.Led.update();

  TempSenseUpdate();
  
  I2C_PrepareData(&LTChannelA, &LTChannelB);
}

