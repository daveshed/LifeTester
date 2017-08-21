#include <SPI.h>
#include <Wire.h>
#include "LEDFlash.h"
#include "ADS1286.h"
#include "MAX6675.h"
#include "Config.h"

void setup()
{ 
  #if DEBUG
    //OPTION DEBUG: SERIAL PORT COMMUNICATION WITH PC
    Serial.begin(9600);
  #endif
    
  //OPTION B: I2C COMMUNICATION WITH MASTER ARDUINO
  Wire.begin(I2C_ADDRESS);           // join I2C bus with address specified (see header)
  Wire.onRequest(I2C_TransmitData); // register event

  //INITIALISE DAC
  MCP48X2_Init(DAC_CS_PIN);

  //SETUP ADC
  //delay time between switching CS pin and reading data over SPI
  //determines measurement speed/rate.
  LTChannelA.AdcInput.setMicroDelay(200); 
  LTChannelB.AdcInput.setMicroDelay(200);

  //MPP INITIAL SEARCH/SCAN
  IV_Scan(&LTChannelA, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN);
  //initialise DAC to MPP initial guess - channel a
  MCP48X2_Output(DAC_CS_PIN, LTChannelA.IVData.v, LTChannelA.DacChannel);
  IV_Scan(&LTChannelB, V_SCAN_MIN, V_SCAN_MAX, DV_SCAN); 
  //initialise DAC to MPP initial guess - channel b
  MCP48X2_Output(DAC_CS_PIN, LTChannelB.IVData.v, LTChannelB.DacChannel);

  #if DEBUG
    //DATA HEADINGS
    Serial.println();
    Serial.println("Tracking max power point...");
    Serial.println("DACx, ADCx, power, error, LDR, T(C), channel");
  #endif
  
  //SETUP LEDS FOR MAIN LOOP
  LTChannelA.Led.t(50, 50);
  LTChannelB.Led.t(50, 50);

  //set timer ready for measurements
  LTChannelA.timer = LTChannelB.timer = millis(); 
}

void loop()
{
  IV_MpptUpdate(&LTChannelA);
  IV_MpptUpdate(&LTChannelB);

  //LED will update every time the loop runs
  LTChannelA.Led.update();
  LTChannelB.Led.update();

  TSense.update();
  
  I2C_PrepareData();
}
