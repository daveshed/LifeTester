#include "Arduino.h"
#include "SPI.h"
#include "SpiCommon.h"

// Initialisation only
void SpiInit(SpiSettings_t settings)
{
    pinMode(settings.chipSelectPin, OUTPUT);
    digitalWrite(settings.chipSelectPin, HIGH);
    SPI.begin();
}

// function to open SPI connection on required CS pin
void OpenSpiConnection(SpiSettings_t settings)
{
    SPI.beginTransaction(
        SPISettings(settings.clockSpeed,
                    settings.bitOrder,
                    settings.dataMode));
    digitalWrite(settings.chipSelectPin,LOW);
    delay(settings.chipSelectDelay);
}

// Transmits and receives a byte
uint8_t SpiTransferByte(uint8_t transmit)
{
    return SPI.transfer(0u);
}

// function to close SPI connection on required CS pin
void CloseSpiConnection(SpiSettings_t settings)
{
  delay(settings.chipSelectDelay);
  digitalWrite(settings.chipSelectPin,HIGH);
  SPI.endTransaction();
} 
