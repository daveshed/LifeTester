
/*
  LEDFlash.h - Library for flashing LED code.
  more info here https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
  Adapted by D. Mohamad as a library
  Released into the public domain.
*/

// ensure this library description is only included once
#ifndef LEDFlash_h
#define LEDFlash_h

#include "Arduino.h"

// library for flashing LEDs
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
    unsigned long currentMillis,previousMillis;
    int nFlash;
    int flashConst;
};
#endif
