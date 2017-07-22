
/*
  MAX6675.h - Library for SPI communication with MAX6675 thermocouple reader
  more info here https://www.maximintegrated.com/en/products/analog/sensors-and-sensor-interface/MAX6675.html
  written by D. Mohamad
  Released into the public domain.
*/

#include "MAX6675.h"
#include "Arduino.h"
#include <SPI.h>

//class member global variable
unsigned int MAX_CS_pin; //chip select pin  

//constructors
MAX6675::MAX6675(unsigned int pin)
{
  //assign chip select to pin output when the class is created. 
  MAX_CS_pin = pin;
  previousMillis = 0;
  raw_data = 0;
  T_deg_C = 0;
  t_delay = 500;
  pinMode (MAX_CS_pin, OUTPUT);
  digitalWrite(MAX_CS_pin,HIGH);
  SPI.begin();
}

//function to read state of ADC and return raw data
unsigned int MAX6675::measure(void)
{
  unsigned int data=0;
  byte MSB,LSB;
  SPI.beginTransaction(SPISettings(4300000, MSBFIRST, SPI_MODE0));

  //now write to DAC
  // take the CS pin low to select the chip:
  digitalWrite(MAX_CS_pin,LOW);
  delay(1);
  //send in the address and value via SPI:
  //note that we read 16bits but there are only 12 containing data 
  MSB = SPI.transfer(0x00); //most sig byte
  LSB = SPI.transfer(0x00); //least sig byte

  data = (MSB<<8 | LSB)>>3;

  delay(1);
  // take the CS pin high to de-select the chip:
  digitalWrite(MAX_CS_pin,HIGH);
  SPI.endTransaction();
  return(data);
}

void MAX6675::update(void)
{
  currentMillis = millis();
  if (currentMillis - previousMillis > t_delay)
  {
    raw_data = measure(); 
    T_deg_C = 1023.75/4096*raw_data;
    previousMillis = currentMillis;
  }  

}
