/*
 * Testing functions to read data from the MX7705 ADC. Added feature to read from 
 * whichever channel that you like.
 * this version uses an external clock from the Arduino pin3 (pin 5 really). Is this more reliable
*/
#include "Arduino.h"
#include "Config.h"
#include "MX7705.h"
#include "Print.h"
#include "SPI.h"

const uint8_t AdcCsPin = ADC_CS_PIN;
const uint8_t LedPin = LED_A_PIN;
const uint8_t ADCChannel = 0;
//const uint8_t gain = 0; //note this isn't gain but the index of the gain table

void setup()
{ 
  Serial.begin(9600);
  SPI.begin();
  MX7705_Init(AdcCsPin, ADCChannel);
  Serial.print("getting gain...");
  Serial.println(MX7705_GetGain(AdcCsPin, ADCChannel));
  Serial.println("setting gain...");
  MX7705_SetGain(AdcCsPin, 0, ADCChannel);
  Serial.print("getting gain...");
  Serial.println(MX7705_GetGain(AdcCsPin, ADCChannel));

  pinMode(LedPin, OUTPUT); //LED select
  digitalWrite(LedPin, LOW);
 
}

void loop()
{
  static uint32_t counter = 0;
  uint16_t ADCData = 0;
  bool error_flag;
  ADCData = MX7705_ReadData(AdcCsPin, ADCChannel);

  if (MX7705_GetError())
  {
    digitalWrite(LedPin, HIGH);
    
  }
  else
  {
    Serial.println(ADCData);
  }
  
  Serial.flush();
  delay(50);
}
