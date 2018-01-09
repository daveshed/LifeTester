/*
 * The purpose of this module is to wrap hardward specific functions from different board revisions
 * into useful functions(abstraction) that are defined at compile time.
 */
#include "Config.h"
#include "IoWrapper.h"
#include "MCP48X2.h"
#include "MX7705.h"
// #ifdef DEBUG
  #include "Arduino.h"
// #endif
#include "TC77.h"

/////////////////
//DAC functions//
/////////////////
void DacInit(void)
{
  MCP48X2_Init(DAC_CS_PIN);
  #if DEBUG
    if (DacGetErrmsg())
    {
      Serial.println("Dac error condition on init.");
    }
  #endif
}

void DacSetOutput(uint8_t output, char channel)
{
  MCP48X2_Output(output, channel);
  #if DEBUG
    Serial.print("Setting Dac output to ");
    Serial.println(output);
  #endif 
}

void DacSetGain(char requestedGain)
{
  MCP48X2_SetGain(requestedGain);
}

char DacGetGain(void)
{
  return MCP48X2_GetGain();
}

uint8_t DacGetErrmsg(void)
{
  return MCP48X2_GetErrmsg();
}

/////////////////
//Adc functions//
/////////////////
void AdcInit(void)
{
  MX7705_Init(ADC_CS_PIN, 0U);
  MX7705_Init(ADC_CS_PIN, 1U);
}

// TODO: remove references to ADS1286 and MAX6675
uint16_t AdcReadData(const char channel)
{
  if (channel == 'a')
  {
    const uint16_t dataA = MX7705_ReadData(0u);
    #if DEBUG
      Serial.print("Adc data ch A = ");
      Serial.println(dataA);
    #endif
    return dataA;
  }
  else if (channel == 'b')
  {
    const uint16_t dataB = MX7705_ReadData(1u);
    #if DEBUG
      Serial.print("Adc data ch B = ");
      Serial.println(dataB);
    #endif
    return dataB;
  }
  else
  {
    #if DEBUG
      Serial.println("Error: Invalid ADC channel selected.");
    #endif
    return 0;
  }
}

bool AdcGetError(void)
{
  #if DEBUG
    Serial.println("Error: Cannot read Adc error");
  #endif
  return MX7705_GetError();
}

uint8_t AdcGetGain(const char channel)
{
  #if DEBUG
    Serial.println("Error: Cannot read gain");
  #endif
  if (channel == 'a')
  {
    return MX7705_GetGain(0u);
  }
  else if (channel == 'b')
  {
    return MX7705_GetGain(1u);
  }
  else
  {
    #if DEBUG
      Serial.println("Error: Invalid ADC channel selected.");
    #endif
    return 0;
  }
}

void AdcSetGain(const uint8_t gain, const char channel)
{
  #if DEBUG
    Serial.println("Error: Cannot set gain");
  #endif
  if (channel == 'a')
  {
    MX7705_SetGain(gain, 0u);
  }
  else if (channel == 'b')
  {
    MX7705_SetGain(gain, 0u);
  }
  else
  {
    #if DEBUG
      Serial.println("Error: Invalid ADC channel selected.");
    #endif
  }
}

////////////////////////////////
//Temperature sensor functions//
////////////////////////////////
void TempSenseInit(void)
{
  TC77_Init(TEMP_CS_PIN);
}

void TempSenseUpdate(void)
{
  TC77_Update();
}

uint16_t TempGetRawData(void)
{
  TC77_GetRawData();
}

float TempReadDegC(void)
{
  return TC77_ConvertToTemp(TC77_GetRawData());
}

bool TempGetError(void)
{
  return TC77_GetError();
}

