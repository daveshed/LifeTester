#ifndef SPICOMMON_H
#define SPICOMMON_H

#ifdef _cplusplus
extern "C"{
#endif
#include <stdint.h>

typedef struct SpiSettings_s{
    uint8_t  chipSelectPin;
    uint16_t chipSelectDelay;
    uint32_t clockSpeed;
    uint8_t  bitOrder;
    uint8_t  dataMode;
} SpiSettings_t;

// Initialisation only
void SpiInit(SpiSettings_t settings);

// Function to open SPI connection on required CS pin
void OpenSpiConnection(SpiSettings_t settings);

// Transmits and receives a byte
uint8_t SpiTransferByte(uint8_t transmit);

// Function to close SPI connection on required CS pin
void CloseSpiConnection(SpiSettings_t settings);

#ifdef _cplusplus
}
#endif
#endif