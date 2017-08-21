/*
 * helpful info on making a library herehttps://www.arduino.cc/en/Hacking/libraryTutorial
*/

#include "ADS1286.h"
#include "Arduino.h"
#include <SPI.h>

// Class Member Variables

unsigned int ADC_CS_pin; //chip select pin
int microSecDelay = 1000; //default setting 1ms

//constructors
//initialise with certain things when we create it
ADS1286::ADS1286(unsigned int pin)
{
	//assign chip select to pin output when the class is created.	
	ADC_CS_pin = pin;	  
  	pinMode (ADC_CS_pin, OUTPUT);
  	digitalWrite(ADC_CS_pin,HIGH);
	SPI.begin();

}

//method to read state of ADC
unsigned int ADS1286::readInputAverage(int nReadings)
{
	unsigned int ADCvalSum=0;
	byte MSB,LSB;
	//now read from ADC
	SPI.beginTransaction(SPISettings(20000, MSBFIRST, SPI_MODE0)); 	//open connection
	
	//take several readings and average - ignore first reading. could be dodgy
	for (int i=0 ; i < (nReadings+1) ; i++)
	{
		// take the CS pin low to select the chip:
		digitalWrite(ADC_CS_pin,LOW);
		delayMicroseconds(microSecDelay);
		//  send in the address and value via SPI:

		MSB = SPI.transfer(0x00); //most sig byte
		LSB = SPI.transfer(0x00); //least sig byte

		//now get rid of the first three digits (most sig bits)
		//combine with the LSB
		//shift everything right one to get rid of junk least sig bit
		if (i>0) //ignore first one
			ADCvalSum += ((MSB & 0x3e)<<8 | LSB) >> 1;

		delayMicroseconds(microSecDelay);
		// take the CS pin high to de-select the chip:
		digitalWrite(ADC_CS_pin,HIGH);
	}

	SPI.endTransaction(); //close connection
	return(ADCvalSum/nReadings);
}

//method to read state of ADC
unsigned int ADS1286::readInputSingleShot()
{
	unsigned int ADCval;
	byte MSB,LSB;
	//now read from ADC
	SPI.beginTransaction(SPISettings(20000, MSBFIRST, SPI_MODE0)); 	//open connection
	
	// take the CS pin low to select the chip:
	digitalWrite(ADC_CS_pin,LOW);
	delayMicroseconds(microSecDelay);
	//  send in the address and value via SPI:

	MSB = SPI.transfer(0x00); //most sig byte
	LSB = SPI.transfer(0x00); //least sig byte

	//now get rid of the first three digits (most sig bits)
	//combine with the LSB
	//shift everything right one to get rid of junk least sig bit
	ADCval= ((MSB & 0x1f)<<8 | LSB) >> 1;

	delayMicroseconds(microSecDelay);
	// take the CS pin high to de-select the chip:
	digitalWrite(ADC_CS_pin,HIGH);
	

	SPI.endTransaction(); //close connection
	
 	/*byte mask;
 	mask = 1;
 	mask <<= 7;

 	for (int i = 0; i < 8; i++)
 	{
 		if (mask & MSB)
 			Serial.print(1);
 		else
 			Serial.print(0);
 		mask >>= 1;
 	}

 	Serial.print(" ");
 	
 	mask = 1;
 	mask <<= 7;
 	for (int i = 0; i < 8; i++)
 	{
 		if (mask & LSB)
 			Serial.print(1);
 		else
 			Serial.print(0);
 		mask >>= 1;
 	}
 	
 	Serial.print(", ");
 	Serial.println(ADCval);
*/
	return(ADCval);
}

void ADS1286::setMicroDelay(int time)
{
	microSecDelay = time;
}
