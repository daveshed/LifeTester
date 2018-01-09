
/*
  LedFlash.h - Library for flashing LED code.
  more info here https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
  Adapted by D. Mohamad as a library
*/

// ensure this library description is only included once
#ifndef LEDFLASH_H
#define LEDFLASH_H

#include <stdint.h>
#include <stdbool.h>

class Flasher
{
  public:
    Flasher(uint8_t);
    void t(uint32_t onTime, uint32_t offTime);
    void update(void);
    void on(void);
    void off(void);
    void stopAfter(int16_t);
    void keepFlashing(void);
  private:
    // Class Member Variables
    // These are initialized at startup
    uint8_t  ledPin;     // the number of the LED pin
    uint32_t onTime;     // milliseconds of on-time
    uint32_t offTime;    // milliseconds of off-time

    // These maintain the current state
    bool     ledState;       // led output state
    uint32_t currentMillis;
    uint32_t previousMillis;
    uint16_t nFlash;
    bool     flashConst;
};

#endif
