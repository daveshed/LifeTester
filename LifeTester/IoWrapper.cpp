/*
 * The purpose of this module is to wrap hardward specific functions from different board revisions
 * into useful functions(abstraction) that are defined at compile time.
 */
#include "Config.h"
#include "IoWrapper.h"
#include "MCP4802.h"
#include "MX7705.h"
#include "Arduino.h"
#include "TC77.h"

/////////////////
//DAC functions//
/////////////////
void DacInit(void)
{
  MCP4802_Init(DAC_CS_PIN);
}

void DacSetOutput(uint8_t output, chSelect_t channel)
{
  MCP4802_Output(output, channel);
  #if DEBUG
    Serial.print("Setting Dac output to ");
    Serial.println(output);
  #endif 
}

void DacSetGain(gainSelect_t requestedGain)
{
  MCP4802_SetGain(requestedGain);
}

gainSelect_t DacGetGain(void)
{
  return MCP4802_GetGain();
}

/////////////////
//Adc functions//
/////////////////
void AdcInit(void)
{
  MX7705_Init(ADC_CS_PIN, 0U);
  MX7705_Init(ADC_CS_PIN, 1U);
}

uint16_t AdcReadData(const uint8_t channel)
{
  if (channel == 0U)
  {
    const uint16_t dataA = MX7705_ReadData(0U);
    #if DEBUG
      Serial.print("Adc data ch A = ");
      Serial.println(dataA);
    #endif
    return dataA;
  }
  else if (channel == 1U)
  {
    const uint16_t dataB = MX7705_ReadData(1U);
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
    return 0U;
  }
}

bool AdcGetError(void)
{
  #if DEBUG
    Serial.println("Error: Cannot read Adc error");
  #endif
  return MX7705_GetError();
}

uint8_t AdcGetGain(const uint8_t channel)
{
  #if DEBUG
    Serial.println("Error: Cannot read gain");
  #endif
  if (channel == 0U)
  {
    return MX7705_GetGain(0u);
  }
  else if (channel == 0U)
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

void AdcSetGain(const uint8_t gain, const uint8_t channel)
{
  #if DEBUG
    Serial.println("Error: Cannot set gain");
  #endif
  if (channel == 0U)
  {
    MX7705_SetGain(gain, 0u);
  }
  else if (channel == 0U)
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

