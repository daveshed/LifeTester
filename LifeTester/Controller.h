#ifndef CONTROLLER_H
#define CONTROLLER_H

#ifdef _cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "LifeTesterTypes.h"

void I2C_ClearArray(uint8_t byteArray[], const uint8_t numBytes);
void I2C_PackIntToBytes(const uint64_t data, uint8_t byteArray[], const uint8_t numBytes);
void I2C_PrintByteArray(const uint8_t byteArray[], const uint16_t n);
void I2C_TransmitData(void);
void I2C_PrepareData(LifeTester_t const *const LTChannelA, LifeTester_t const *const LTChannelB);

#ifdef _cplusplus
}
#endif

#endif //include guard