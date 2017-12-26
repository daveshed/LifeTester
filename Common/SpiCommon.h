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

// Opens the Spi bus only - needs to be called once during setup
void SpiBegin(void);

// Initialises the chip select pin only - must be called during setup
void InitChipSelectPin(const uint8_t pin);

// Function to open SPI connection on required CS pin
void OpenSpiConnection(const SpiSettings_t *settings);

// Transmits and receives a byte
uint8_t SpiTransferByte(const uint8_t transmit);

// Function to close SPI connection on required CS pin
void CloseSpiConnection(const SpiSettings_t *settings);

#ifdef _cplusplus
}
#endif
#endif