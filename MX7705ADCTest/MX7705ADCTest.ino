/*
 * Testing functions to read data from the MX7705 ADC. Added feature to read from 
 * whichever channel that you like.
 * this version uses an external clock from the Arduino pin3 (pin 5 really). Is this more reliable
 */

#include <SPI.h>
const uint8_t AdcAdcCsPin = 9;
const uint8_t ADCChannel = 0;
const uint8_t gain = 1; //note this isn't gain but the index of the gain table

void setup()
{ 
  Serial.begin(9600);
  SPI.begin();
  MX7705Init(AdcCsPin, ADCChannel);
  Serial.print("getting gain...");
  Serial.println(MX7705GetGain(AdcAdcCsPin, ADCChannel));
  Serial.println("setting gain...");
  MX7705SetGain(AdcCsPin, 3, ADCChannel);
  Serial.print("getting gain...");
  Serial.println(MX7705GetGain(AdcCsPin, ADCChannel));
}



void loop()
{
  static uint32_t counter = 0;
  uint16_t ADCData = 0;
  bool timeout_flag;
  ADCData = MX7705ReadData(AdcCsPin, &timeout_flag, ADCChannel);
  if (timeout_flag)
  {
    Serial.println("measurement timeout.");
  }
  else
  {
    Serial.println(ADCData);
  }
  
  Serial.flush();
  delay(100);
}
