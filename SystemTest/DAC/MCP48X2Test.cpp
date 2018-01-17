#include "Config.h"
#include "MCP4802.h"
#include "SPI.h"

const uint8_t DacCsPin = DAC_CS_PIN;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin();
  MCP4802_Init(DacCsPin);
  MCP4802_Output(0u, 'a');
  MCP4802_Output(0u, 'b');
}

void loop()
{
  static uint8_t dacCodeRequest = 0;
  if (dacCodeRequest > 100)
  {
    dacCodeRequest = 0;
  }
  MCP4802_Output(dacCodeRequest, 'a');
  MCP4802_Output(dacCodeRequest, 'b');
  Serial.print(dacCodeRequest);
  Serial.print(" ");
  Serial.println((float)dacCodeRequest * 2.048 / 255.0);
  delay(250);
  dacCodeRequest++;
}
