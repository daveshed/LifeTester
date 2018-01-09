/*
 * Testing functions to read data from the MX7705 ADC. Added feature to read from 
 * whichever channel that you like. This version uses an external clock from
 * the Arduino pin3 (IC pin 5). Is this more reliable than the internal clock.
*/
#include "Arduino.h"
#include "Config.h"
#include "MX7705.h"
#include "Print.h"
#include "SPI.h"

const uint8_t AdcCsPin = ADC_CS_PIN;
const uint8_t LedPin = LED_A_PIN;

void setup()
{ 
    Serial.begin(9600);
    // SpiBegin();
    SPI.begin();
    Serial.print("Initialising ADC on pin ");
    Serial.println(AdcCsPin);
    MX7705_Init(AdcCsPin, 0U);
    MX7705_Init(AdcCsPin, 1U);
    Serial.println("Done");

    
    Serial.print("getting gain...");
    Serial.println(MX7705_GetGain(0U));
    
    // Serial.println("setting gain...");
    // MX7705_SetGain(0U, 0U);
    
    // Initialising LED for error condition
    pinMode(LedPin, OUTPUT);
    digitalWrite(LedPin, LOW);

}

void loop()
{
    const uint16_t adcDataCh0 = MX7705_ReadData(0U);
    const uint16_t adcDataCh1 = MX7705_ReadData(1U);

    if (MX7705_GetError())
    {
        digitalWrite(LedPin, HIGH);
    }
    else
    {
        Serial.print("Ch0 = ");
        Serial.print(adcDataCh0);
        Serial.print(", Ch1 = ");
        Serial.println(adcDataCh1);
    }

    Serial.flush();
    delay(50);
}
