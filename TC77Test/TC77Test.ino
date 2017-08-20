#include <SPI.h>
const uint8_t CSPin = 8;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(CSPin, OUTPUT);
  digitalWrite(CSPin,HIGH);
  SPI.begin();
}

void loop()
{
  Serial.println(TC77ReadTemp(CSPin));
  delay(1000);
}
