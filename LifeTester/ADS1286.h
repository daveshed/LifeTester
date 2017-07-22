// ensure this library description is only included once
#ifndef ADS1286_h
#define ADS1286_h

#include "Arduino.h"

class ADS1286
{
	public:
		ADS1286(unsigned int); //initialise
		unsigned int readInputAverage(int);
		unsigned int readInputSingleShot();
		void setMicroDelay(int);
  		
	private:
		// Class Member Variables
  		unsigned int ADCvalSum, ADCval;
  		byte MSB,LSB;
	  	unsigned int ADC_CS_pin; //chip select pin
	  	int nReadings; //number of readings to average
	  	int microSecDelay; // delay between switching CS pin and reading from SPI in microsec
};

#endif