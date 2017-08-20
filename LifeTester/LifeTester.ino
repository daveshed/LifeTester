#include "lifetester.h"
#include "IV.h"

void setup()
{ 
  #if DEBUG
    //OPTION DEBUG: SERIAL PORT COMMUNICATION WITH PC
    Serial.begin(9600);
  #endif
    
  //OPTION B: I2C COMMUNICATION WITH MASTER ARDUINO
  Wire.begin(I2CAddress);           // join I2C bus with address specified (see header)
  Wire.onRequest(I2C_TransmitData); // register event

  //INITIALISE DAC
  MCP48X2_Init(Dac_CSPin);

  //SETUP ADC
  //delay time between switching CS pin and reading data over SPI
  //determines measurement speed/rate.
  LTChannelA.AdcInput.setMicroDelay(200); 
  LTChannelB.AdcInput.setMicroDelay(200);

  //MPP INITIAL SEARCH/SCAN
  IV_Scan(&LTChannelA, VScanMin, VScanMax, dVScan);
  //initialise DAC to MPP initial guess - channel a
  MCP48X2_Output(Dac_CSPin, LTChannelA.IVData.v, LTChannelA.DacChannel);
  IV_Scan(&LTChannelB, VScanMin, VScanMax, dVScan); 
  //initialise DAC to MPP initial guess - channel a
  MCP48X2_Output(Dac_CSPin, LTChannelB.IVData.v, LTChannelB.DacChannel);

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
