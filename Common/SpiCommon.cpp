#include "Arduino.h"
#include "SPI.h"
#include "SpiCommon.h"

void SpiBegin(void)
{
    // open the spi bus
    SPI.begin();
}

void InitChipSelectPin(uint8_t pin)
{
    // set pin to output mode
    pinMode(pin, OUTPUT);
    // set pin high which deactivates spi line for that device
    digitalWrite(pin, HIGH);
}

// function to open SPI connection on required CS pin
void OpenSpiConnection(const SpiSettings_t *settings)
{
    SPI.beginTransaction(
        SPISettings(settings->clockSpeed,
                    settings->bitOrder,
                    settings->dataMode));
    digitalWrite(settings->chipSelectPin, LOW);
    delay(settings->chipSelectDelay);
}

// Transmits and receives a byte
uint8_t SpiTransferByte(const uint8_t transmit)
{
    return SPI.transfer(0u);
}

// function to close SPI connection on required CS pin
void CloseSpiConnection(const SpiSettings_t *settings)
{
  delay(settings->chipSelectDelay);
  digitalWrite(settings->chipSelectPin, HIGH);
  SPI.endTransaction();
} 
