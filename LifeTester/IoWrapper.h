#ifndef IOWRAPPER_H
#define IOWRAPPER_H

#ifdef _cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

void DacInit(void);
void DacSetOutput(uint8_t output, char channel);
void DacSetGain(char requestedGain);
char DacGetGain(void);
uint8_t DacGetErrmsg(void);
void AdcInit(void);
uint16_t AdcReadData(const char channel);
bool AdcGetError(void);
uint8_t AdcGetGain(const char channel);
void AdcSetGain(const uint8_t gain, const char channel);
void TempSenseInit(void);
void TempSenseUpdate(void);
uint16_t TempGetRawData(void);
float TempReadDegC(void);
bool TempGetError(void);

#ifdef _cplusplus
}
#endif

#endif