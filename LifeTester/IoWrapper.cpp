/*
 The purpose of this module is to wrap hardware specific functions from the
 different peripherals into useful functions(abstraction) that act on the life-
 tester object.
 */
#include "Arduino.h"
#include "Config.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "MCP4802.h"
#include "MX7705.h"
#include "TC77.h"

// records a copy of the last output set on the dac for each channel.
static uint8_t dacOutput[nChannels];

/////////////////
//DAC functions//
/////////////////
void DacInit(void)
{
    MCP4802_Init(DAC_CS_PIN);
}

void DacSetOutputToThisVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(lifeTester->data.vThis, lifeTester->io.dac);
}

void DacSetOutputToNextVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(lifeTester->data.vNext, lifeTester->io.dac);
}

void DacSetOutputToScanVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(lifeTester->data.vScan, lifeTester->io.dac);
}

void DacSetOutput(uint8_t output, chSelect_t ch)
{
    MCP4802_Output(output, ch);
    #if DEBUG
        Serial.print("Setting Dac channel ");
        Serial.print((uint8_t)ch);
        Serial.print(" output to ");
        Serial.println(output);
    #endif
    
    // keep a copy of last voltage set on this channel
    dacOutput[ch] = output;
}

uint8_t DacGetOutput(LifeTester_t const *const lifeTester)
{
    const chSelect_t ch = lifeTester->io.dac;
    return dacOutput[ch];
}

bool DacOutputSetToThisVoltage(LifeTester_t const *const lifeTester)
{
    return DacGetOutput(lifeTester) == lifeTester.data.vThis;
}

bool DacOutputSetToNextVoltage(LifeTester_t const *const lifeTester)
{
    return DacGetOutput(lifeTester) == lifeTester.data.vNext;
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

uint16_t AdcReadLifeTesterCurrent(LifeTester_t const *const lifeTester)
{
    return AdcReadData(lifeTester->io.adc);
}

uint16_t AdcReadData(uint8_t channel)
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

