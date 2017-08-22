/*
 * The purpose of this module is to wrap hardward specific functions from different board revisions
 * into useful functions(abstraction) that are defined at compile time.
 */
#include "Config.h"
#include "ADS1286.h"
#include "MAX6675.h"

///////////////////
//Class instances// - instead of making these global, why not make them static within the relevant wrapper?
///////////////////
#ifdef BOARD_REV_0
  MAX6675 Max6675(TEMP_CS_PIN);
  static ADS1286 Ads1286_chA(ADC_A_CS_PIN);
  static ADS1286 Ads1286_chB(ADC_B_CS_PIN);
#endif

/////////////////
//DAC functions//
/////////////////
void DacInit(void)
{
  MCP48X2_Init(DAC_CS_PIN);
}

void DacSetOutput(uint8_t output, char channel)
{
  MCP48X2_Output(DAC_CS_PIN, output, channel);
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

void AdcInit(void)
{
  #if defined(BOARD_REV_0)
    Ads1286_chA.setMicroDelay(ADC_CS_DELAY);
    Ads1286_chB.setMicroDelay(ADC_CS_DELAY);
  #elif defined(BOARD_REV_1)
    MX7705_Init(ADC_CS_PIN, 0);
    MX7705_Init(ADC_CS_PIN, 1);
  #endif
}

uint16_t AdcReadData(const char channel)
{
  #if defined(BOARD_REV_0)
    if (channel == 'a')
    {
      return Ads1286_chA.readInputSingleShot();  
    }
    else if (channel == 'b')
    {
      return Ads1286_chB.readInputSingleShot();  
    }
    else
    {
      #if DEBUG
        Serial.println("Error: Invalid ADC channel selected.");
      #endif
    }
  #elif defined(BOARD_REV_1)
    if (channel == 'a')
    {
      return MX7705_ReadData(ADC_CS_PIN, 0);
    }
    else if (channel == 'b')
    {
      return MX7705_ReadData(ADC_CS_PIN, 1);  
    }
    else
    {
      #if DEBUG
        Serial.println("Error: Invalid ADC channel selected.");
      #endif
      return 0;
    }
  #endif
}

bool AdcGetError(void)
{
  #if defined(BOARD_REV_0)
    //ADS1286 library has no error checking
    #if DEBUG
      Serial.println("Error: Cannot read Adc error");
    #endif
    return false;
  #elif defined(BOARD_REV_1)
    return MX7705_GetError();
  #endif
}

uint8_t AdcGetGain(const char channel)
{
  #if defined(BOARD_REV_0)
    #if DEBUG
      Serial.println("Error: Cannot read gain");
    #endif
    return 1;
  #elif defined(BOARD_REV_1)
    if (channel == 'a')
    {
      return MX7705_GetGain(ADC_CS_PIN, 0);
    }
    else if (channel == 'b')
    {
      return MX7705_GetGain(ADC_CS_PIN, 1);
    }
    else
    {
      #if DEBUG
        Serial.println("Error: Invalid ADC channel selected.");
      #endif
      return 0;
    }
  #endif 
}

void AdcSetGain(const uint8_t gain, const char channel)
{
  #if defined(BOARD_REV_0)
    #if DEBUG
      Serial.println("Error: Cannot set gain");
    #endif
    return 1;
  #elif defined(BOARD_REV_1)
    if (channel == 'a')
    {
      MX7705_SetGain(ADC_CS_PIN, gain, 0);
    }
    else if (channel == 'b')
    {
      MX7705_SetGain(ADC_CS_PIN, gain, 0);
    }
    else
    {
      #if DEBUG
        Serial.println("Error: Invalid ADC channel selected.");
      #endif
      return 0;
    }
  #endif 
}

void TempSenseUpdate(void)
{
  #if defined(BOARD_REV_0)
    Max6675.update();
  #elif defined(BOARD_REV_1)
    TC77_Update(TEMP_CS_PIN);
  #endif
}

uint16_t TempGetRawData(void)
{
  #if defined(BOARD_REV_0)
    return Max6675.raw_data;
  #elif defined(BOARD_REV_1)
    TC77_GetRawData();
  #endif
}

float TempReadDegC(void)
{
  #if defined(BOARD_REV_0)
    return Max6675.T_deg_C;
  #elif defined(BOARD_REV_1)
    return TC77_ConvertToTemp(TC77_GetRawData());
  #endif
}

bool TempGetError(void)
{
  #if defined(BOARD_REV_0)
    return false;
  #elif defined(BOARD_REV_1)
    return TC77_GetError();
  #endif
}

