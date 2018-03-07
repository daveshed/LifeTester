#ifndef IOWRAPPER_H
#define IOWRAPPER_H

#ifdef _cplusplus
extern "C"
{
#endif

#include "MCP4802.h" // dac types
#include "LifeTesterTypes.h"
#include <stdint.h>
#include <stdbool.h>

void DacInit(void);
void DacSetOutputToThisVoltage(LifeTester_t const *const lifeTester);
void DacSetOutputToNextVoltage(LifeTester_t const *const lifeTester);
void DacSetOutputToScanVoltage(LifeTester_t const *const lifeTester);
void DacSetOutput(uint8_t output, chSelect_t ch);
uint8_t DacGetOutput(LifeTester_t const *const lifeTester);
bool DacOutputSetToThisVoltage(LifeTester_t const *const lifeTester);
bool DacOutputSetToNextVoltage(LifeTester_t const *const lifeTester);
void DacSetGain(gainSelect_t requestedGain);
gainSelect_t DacGetGain(void);
void AdcInit(void);
uint16_t AdcReadLifeTesterCurrent(LifeTester_t const *const lifeTester);
uint16_t AdcReadData(uint8_t channel);
bool AdcGetError(void);
uint8_t AdcGetGain(const uint8_t channel);
void AdcSetGain(const uint8_t gain, const uint8_t channel);
void TempSenseInit(void);
void TempSenseUpdate(void);
uint16_t TempGetRawData(void);
float TempReadDegC(void);
bool TempGetError(void);

#ifdef _cplusplus
}
#endif

#endif