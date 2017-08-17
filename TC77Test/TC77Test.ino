#include <SPI.h>
const uint8_t CSPin = 10;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin();
}

void loop()
{
  Serial.println(TC77ReadTemp(CSPin));
  delay(10);
}
