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
  DacSmu.gainSet(DacGain);
  DacSmu.output('a', 0);
  DacSmu.output('b', 0);
  DacErrmsg = DacSmu.readErrmsg();

  //SETUP ADC
  //delay time between switching CS pin and reading data over SPI
  //determines measurement speed/rate.
  LTChannelA.AdcInput.setMicroDelay(200); 
  LTChannelB.AdcInput.setMicroDelay(200);

  //MPP INITIAL SEARCH/SCAN
  IV_Scan(&LTChannelA, VScanMin, VScanMax, dVScan, DacSmu); 
  DacSmu.output(LTChannelA.DacChannel, LTChannelA.IVData.v); //initialise DAC to MPP initial guess
  IV_Scan(&LTChannelB, VScanMin, VScanMax, dVScan, DacSmu); 
  DacSmu.output(LTChannelB.DacChannel, LTChannelB.IVData.v); //initialise DAC to MPP initial guess  

  #if DEBUG
    //DATA HEADINGS
    Serial.println();
    Serial.println("mode,timer,channel,DACx,ADCx,power,error,LDR,T(C)");
  #endif
  
  //SETUP LEDS FOR MAIN LOOP
  LTChannelA.Led.t(50, 50);
  LTChannelB.Led.t(50, 50);

  //set timer ready for measurements
  LTChannelA.timer = LTChannelB.timer = millis(); 
}

void loop()
{
  IV_MpptUpdate(&LTChannelA, DacSmu);
  IV_MpptUpdate(&LTChannelB, DacSmu);

  //LED will update every time the loop runs
  LTChannelA.Led.update();
  LTChannelB.Led.update();

  TSense.update();
  
  I2C_PrepareData();
}
