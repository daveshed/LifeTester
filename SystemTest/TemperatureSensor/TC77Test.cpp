#include "Arduino.h"
#include "Config.h"
#include "Print.h"
#include "TC77.h"

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);
  TC77_Init(TEMP_CS_PIN);
}

void loop()
{
  TC77_Update();
  uint16_t rawData = TC77_GetRawData();
  Serial.println(rawData);
  Serial.println(TC77_ConvertToTemp(rawData));
  delay(1000);
}
