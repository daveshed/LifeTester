
/*
  LedFlash.h - Library for flashing LED code.
  more info here https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
  Adapted by D. Mohamad as a library
*/

// ensure this library description is only included once
#ifndef LEDFLASH_H
#define LEDFLASH_H

#include "Arduino.h"

class Flasher
{
  public:
    Flasher(int);
    void t(long, long);
    void update();
    void on();
    void off();
    void stopAfter(int);
    void keepFlashing();
  private:
    // Class Member Variables
    // These are initialized at startup
    int ledPin;      // the number of the LED pin
    long OnTime;     // milliseconds of on-time
    long OffTime;    // milliseconds of off-time

    // These maintain the current state
    int ledState;                 // ledState used to set the LED
    unsigned long currentMillis;
    unsigned long previousMillis;
    int nFlash;
    int flashConst;
};

#endif
