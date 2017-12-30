#ifndef MX7705_H
#define MX7705_H

#ifdef _cplusplus
extern "C" {
#endif

#include "SpiCommon.h"
#include <stdint.h>

/*
 enum containing all available registers that can be selected from RS0-2 bits
 of the comms register.
*/
typedef enum RegisterSelection_e {
    CommsReg,
    SetupReg,
    ClockReg,
    DataReg,
    TestReg,
    NoOperation,
    OffsetReg,
    GainReg,
    NumberOfEntries
} RegisterSelection_t;

void MX7705_Init(const uint8_t pin, const uint8_t channel);
bool MX7705_GetError(void);
uint16_t MX7705_ReadData(const uint8_t channel);
uint8_t MX7705_GetGain(const uint8_t channel);
void MX7705_SetGain(const uint8_t requiredGain, const uint8_t channel);
void MX7705_GainUp(const uint8_t channel);

#ifdef _cplusplus
}
#endif
#endif