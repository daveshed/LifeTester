#include "Arduino.h"
#include "Config.h"
#include "LedFlash.h"

Flasher LedA(LED_A_PIN);
Flasher LedB(LED_B_PIN);

void setup()
{
 LedA.t(100, 100);
}

void loop()
{
	LedA.update();
	LedB.update();
}
