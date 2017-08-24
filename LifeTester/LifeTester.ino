#include <SPI.h>
#include <Wire.h>
#include "LEDFlash.h"
#include "LifeTesterTypes.h"
#include "Config.h"

void setup()
{ 
  //SERIAL PORT COMMUNICATION WITH PC VIA UART
  Serial.begin(9600);
    
  //OPTION B: I2C COMMUNICATION WITH MASTER ARDUINO
  Wire.begin(I2C_ADDRESS);          // join I2C bus with address specified (see header)
  Wire.onRequest(I2C_TransmitData); // register event

  //INITIALISE I/O
  #if DEBUG
    Serial.println("Initialising IO...");
  #endif
    
  DacInit();
  AdcInit();

  #if DEBUG
    Serial.println("Scanning for MPP...");
  #endif
  //MPP INITIAL SEARCH/SCAN
  IV_Scan(&LTChannelA, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN);
  //initialise DAC to MPP initial guess - channel a
  DacSetOutput(LTChannelA.IVData.v, LTChannelA.channel);
 
  IV_Scan(&LTChannelB, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN); 
  //initialise DAC to MPP initial guess - channel b
  DacSetOutput(LTChannelB.IVData.v, LTChannelB.channel);

  //DATA HEADINGS
  Serial.println();
  Serial.println("Tracking max power point...");
  Serial.println("DACx, ADCx, power, error, Light Sensor, T(C), channel");
  
  //SETUP LEDS FOR MAIN LOOP
  LTChannelA.Led.t(50, 50);
  LTChannelB.Led.t(50, 50);

  //set timer ready for measurements
  LTChannelA.timer = LTChannelB.timer = millis();

  #if DEBUG
    Serial.println("Finished setup. Entering main loop.");
  #endif
}

void loop()
{
  IV_MpptUpdate(&LTChannelA);
  IV_MpptUpdate(&LTChannelB);

  //LED will update every time the loop runs
  LTChannelA.Led.update();
  LTChannelB.Led.update();

  TempSenseUpdate();
  
  I2C_PrepareData();
}
