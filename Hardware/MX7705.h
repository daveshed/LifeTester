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

/*
 * Setup the MX7705 in unipolar, unbuffered mode. Allow the user to 
 * select which channel they want. 0/1 = AIN1+ to AIN1-/AIN2+ to AIN2-.
 */
void MX7705_Init(const uint8_t pin, const uint8_t channel);

// Gets the error condition
bool MX7705_GetError(void);

/*
 Reads two bytes from the ADC and convert to a 16Bit unsigned int representing
 the converted voltage.
 */
uint16_t MX7705_ReadData(const uint8_t channel);

// Reads the currently set gain of a given channel
uint8_t MX7705_GetGain(const uint8_t channel);

/*
 Sets the gain of the MX7705. Note that gain is set as an unsigned int from
 0 - 7 inclusive
 */
void MX7705_SetGain(const uint8_t requiredGain, const uint8_t channel);

// Increases the gain of selected channel by one step
void MX7705_IncrementGain(const uint8_t channel);

// Decreases the gain of selected channel by one step
void MX7705_DecrementGain(const uint8_t channel);

#ifdef _cplusplus
}
#endif
#endif