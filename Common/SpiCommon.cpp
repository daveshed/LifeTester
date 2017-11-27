#include "Arduino.h"
#include "SPI.h"
#include "SpiCommon.h"

/*
 * initialisation only
 */
void SpiInit(uint8_t chipSelectPin)
{
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  SPI.begin();
}

/*
 * function to open SPI connection on required CS pin
 */
void OpenSpiConnection(uint8_t chipSelectPin, uint16_t CsDelay, uint32_t clockSpeed, uint8_t bitOrder, uint8_t dataMode)
{
  SPI.beginTransaction(SPISettings(clockSpeed, bitOrder, dataMode));
  digitalWrite(chipSelectPin,LOW);
  delay(CsDelay);
}

/*
 * function to close SPI connection on required CS pin
 */
void CloseSpiConnection(uint8_t chipSelectPin, uint16_t CsDelay)
{
  delay(CsDelay);
  digitalWrite(chipSelectPin,HIGH);
  SPI.endTransaction();
} 
