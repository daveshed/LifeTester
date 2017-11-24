/*
  LedFlash.cpp - Library for flashing LED code.
  more info here https://www.arduino.cc/en/Tutorial/BlinkWithoutDelay
  Adapted by D. Mohamad as a library
  Released into the public domain.
*/

#include "LedFlash.h"
#include "Arduino.h"

//initilize class varibles
int ledPin;      // the number of the LED pin
long OnTime;     // milliseconds of on-time
long OffTime;    // milliseconds of off-time

// These maintain the current state
int ledState;                 // ledState used to set the LED
unsigned long previousMillis, currentMillis;   // will store last time LED was updated
int nFlash;  //number of blinks to complete
int flashConst; //state used to tell us if LED is blinking constantly or for a fixed number

//constructors
//initialise with certain things when we create it
Flasher::Flasher(int pin)
{
  ledPin = pin;
  pinMode(ledPin, OUTPUT);    

  ledState = LOW; 
  previousMillis = 0;
  //initialise to a value so we don't have errors
  //eg if someone tries to update without calling t method first.
  OnTime = 200; 
  OffTime = 800;
  flashConst=1;
}

//Public methods

//change on/off times
void Flasher::t(long OnNew, long OffNew)
{
  OnTime = OnNew;
  OffTime = OffNew;
}

//flash indefinitely method
void Flasher::keepFlashing()
{
  flashConst=1;
}

//flash given number of times
void Flasher::stopAfter(int n)
{
  nFlash=n;
  flashConst=0;
}

//updatestate
void Flasher::update()
{
  //check state of flashConst - do we need to flash?
  if (flashConst==1 || (flashConst==0 && nFlash>=0))
  {
    // check to see if it's time to change the state of the LED
    currentMillis = millis();
     
    if((ledState == HIGH) && (currentMillis - previousMillis >= OnTime))
    {
      ledState = LOW;  // Turn it off
      previousMillis = currentMillis;  // Remember the time
      digitalWrite(ledPin, ledState);  // Update the actual LED
    }
    else if ((ledState == LOW) && (currentMillis - previousMillis >= OffTime))
    {
      ledState = HIGH;  // turn it on
      previousMillis = currentMillis;   // Remember the time
      digitalWrite(ledPin, ledState);   // Update the actual LED

      if (flashConst==0)
        nFlash--; //done a flash therefore decrement counter
    }  
  }

  else
    off();
}

//just turn it on
void Flasher::on()
{
  digitalWrite(ledPin,HIGH);
}

//just turn it off
void Flasher::off()
{
  digitalWrite(ledPin,LOW);
}
