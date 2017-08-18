#include <SPI.h>
#include "MCP48X2.h"
const uint8_t DacCsPin = 10;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin();
  MCP48X2_Init(DacCsPin);
  MCP48X2_Output(DacCsPin, 0, 'a');
  MCP48X2_Output(DacCsPin, 0, 'b');
}

void loop()
{
  static uint8_t dacCodeRequest = 0;
  MCP48X2_Output(DacCsPin, dacCodeRequest, 'a');
  MCP48X2_Output(DacCsPin, dacCodeRequest, 'b');
  Serial.print(dacCodeRequest);
  Serial.print(" ");
  Serial.println((float)dacCodeRequest * 2.048 / 255.0);
  delay(250);
  dacCodeRequest++;
}
