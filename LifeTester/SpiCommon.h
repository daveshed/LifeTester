#ifndef SPICOMMON_H
#define SPICOMMON_H

#ifdef _cplusplus
extern "C"{
#endif
#include <stdint.h>

void SpiInit(uint8_t chipSelectPin);
void OpenSpiConnection(uint8_t chipSelectPin, uint16_t CsDelay, uint32_t clockSpeed, uint8_t bitOrder, uint8_t dataMode);
void CloseSpiConnection(uint8_t chipSelectPin, uint16_t CsDelay);

#ifdef _cplusplus
}
#endif
#endif