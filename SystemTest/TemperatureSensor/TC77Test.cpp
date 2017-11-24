#include "Config.h"
#include "TC77.h"
#include "SPI.h"
#include "SpiCommon.h"

const uint8_t CSPin = TEMP_CS_PIN;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  TC77_Init(CSPin);
  // TODO: Should this be included in SPI common?
  SPI.begin();
}

void loop()
{
  Serial.println(TC77_ConvertToTemp(TC77_GetRawData()));
  delay(1000);
}
