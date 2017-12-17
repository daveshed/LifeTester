/*
  LedFlash.cpp - Library for flashing LED code.
  more info here https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
  Adapted by D. Mohamad as a library
  Released into the public domain.
*/

#include "Arduino.h"
#include "LedFlash.h"
#include "LedFlashPrivate.h"

//initilize class varibles
uint8_t  ledPin;     // the number of the LED pin
uint32_t onTime;     // milliseconds of on-time
uint32_t offTime;    // milliseconds of off-time

// These maintain the current state
uint8_t  ledState;
uint32_t previousMillis;
uint32_t currentMillis;   // will store last time LED was updated
int16_t  nFlash;          //number of blinks to complete
bool     flashConst;      //state used to tell us if LED is blinking constantly or for a fixed number

// Constructor which initialises flasher class with certain things when we create it
Flasher::Flasher(uint8_t pin)
{
    ledPin = pin;
    pinMode(ledPin, OUTPUT);    

    // Initialise led in off state
    ledState = LOW;
    digitalWrite(pin, ledState);

    previousMillis = 0U;
    //initialise to a value so we don't have errors
    //eg if someone tries to update without calling t method first.
    onTime = DEFAULT_ON_TIME; 
    offTime = DEFAULT_OFF_TIME;
    flashConst = true;
}

//change on/off times
void Flasher::t(uint32_t onNew, uint32_t offNew)
{
    onTime = onNew;
    offTime = offNew;
}

// Sets the flasher to flash indefinitely mode
void Flasher::keepFlashing(void)
{
    flashConst = true;
}

// Flash only a given number of times
void Flasher::stopAfter(int16_t n)
{
    nFlash = n;
    flashConst = false;
}

// Update the state of the led depending on time elapsed
void Flasher::update(void)
{
    //check state of flashConst - do we need to flash?
    if (flashConst || (!flashConst && nFlash > 0))
    {
        // check to see if it's time to change the state of the LED
        currentMillis = millis();
        const uint32_t elapsedTime = currentMillis - previousMillis; 

        if((ledState == HIGH) && (elapsedTime > onTime))
        {
            ledState = LOW;  // Turn it off
            previousMillis = currentMillis;  // Remember the time
            digitalWrite(ledPin, ledState);  // Update the actual LED

            if (!flashConst)
            {
                nFlash--; //done a flash therefore decrement counter
            }
        }
        else if ((ledState == LOW) && (elapsedTime > offTime))
        {
            ledState = HIGH;  // turn it on
            previousMillis = currentMillis;   // Remember the time
            digitalWrite(ledPin, ledState);   // Update the actual LED
        }
        else
        {
            // nothing to do here - no transition occuring
        }
    }
}

//just turn it on
void Flasher::on(void)
{
    digitalWrite(ledPin, HIGH);
}

//just turn it off
void Flasher::off(void)
{
    digitalWrite(ledPin, LOW);
}
