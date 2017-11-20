/*
  MAX6675.h - Library for SPI communication with MAX6675 thermocouple reader
  more info here https://www.maximintegrated.com/en/products/analog/sensors-and-sensor-interface/MAX6675.html
  written by D. Mohamad
  Released into the public domain.
*/

// ensure this library description is only included once
#ifndef MAX6675_h
#define MAX6675_h

#include "Arduino.h"

class MAX6675
{
  public:
  	MAX6675(unsigned int);
    void          update(void);
    float         T_deg_C;
    unsigned int  raw_data;
  private:
  	unsigned int  measure(void);
    unsigned int  ADC_CS_pin; //chip select pin
    unsigned long previousMillis, currentMillis;   // will store last time readingss were updated
  	byte          MSB,LSB;
    unsigned int  t_delay; //delay between updates
};

#endif
