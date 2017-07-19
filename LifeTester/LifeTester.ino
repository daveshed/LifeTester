#include "lifetester.h"

void setup()
{ 
  #if DEBUG
    //OPTION DEBUG: SERIAL PORT COMMUNICATION WITH PC
    Serial.begin(9600);
  #endif
    
  //OPTION B: I2C COMMUNICATION WITH MASTER ARDUINO
  Wire.begin(I2C_address);       // join I2C bus with address specified (see header)
  Wire.onRequest(request_event); // register event

  //INITIALISE DAC
  DAC_SMU.gainSet(DACgain);
  DAC_SMU.output('a', 0);
  DAC_SMU.output('b', 0);
  DAC_errmsg = DAC_SMU.readErrmsg();

  //SETUP ADC
  channel_A.ADC_input.setMicroDelay(200); //delay time between switching CS pin and reading data over SPI - determines measurement speed/rate.
  channel_B.ADC_input.setMicroDelay(200);

  //MPP INITIAL SEARCH/SCAN
  MPP_scan(&channel_A, V_scan_min, V_scan_max, dV_scan, DAC_SMU); 
  DAC_SMU.output(channel_A.DAC_channel, channel_A.iv_data.v); //initialise DAC to MPP initial guess
  MPP_scan(&channel_B, V_scan_min, V_scan_max, dV_scan, DAC_SMU); 
  DAC_SMU.output(channel_B.DAC_channel, channel_B.iv_data.v); //initialise DAC to MPP initial guess  

  #if DEBUG
    //DATA HEADINGS
    Serial.println();
    Serial.println("Tracking max power point...");
    Serial.println("DACx, ADCx, power, error, LDR, T(C), channel");
  #endif
  
  //SETUP LEDS FOR MAIN LOOP
  channel_A.Led.t(50, 50);
  channel_B.Led.t(50, 50);

  //set timer ready for measurements
  channel_A.timer = channel_B.timer = millis(); 
}

void loop()
{

  MPP_update(&channel_A, DAC_SMU);
  MPP_update(&channel_B, DAC_SMU);

  //LED will update every time the loop runs
  channel_A.Led.update();
  channel_B.Led.update();

  T_sense.update();
  
  I2C_prepare_data(I2C_data);
}
