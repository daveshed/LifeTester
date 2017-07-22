/*
helpful info on making a library herehttps://www.arduino.cc/en/Hacking/libraryTutorial

*/

#include "MCP4822.h"
#include "Arduino.h"
#include <SPI.h>

//initialise class member variables
unsigned char gain;
unsigned int errmsg;
unsigned int DAC_CS_pin;


//constructors
//initialise with certain things when we create it
MCP4822::MCP4822(unsigned int pin)
{
	//assign chip select to pin output when the class is created.	
	DAC_CS_pin = pin;	  
  	pinMode (DAC_CS_pin, OUTPUT);
  	digitalWrite(DAC_CS_pin,HIGH);
  	SPI.begin();
}

void MCP4822::output(char channel, unsigned int output)
{
	byte MSB,LSB;
	errmsg = 0;
	//only run the rest of the code if binary is in range.
	if (output < 0 || output > 4095)
		errmsg = 1;
	else
	{
		//convert decimal output to binary stored in two bytes
		MSB = (output >> 8) & 0xFF;  //most sig byte
		LSB = output & 0xFF;         //least sig byte

		//apply config bits to the front of MSB
		if (channel=='a' || channel=='A')
		MSB &= 0x7F; //writing a 0 to bit 7.
		else if (channel=='b' || channel=='B')
		MSB |= 0x80; //writing a 1 to bit 7.
		else
		errmsg = 2;

		if (gain=='l' || gain=='L')
		MSB |= 0x20;
		else if (gain=='h' || gain=='H')
		MSB &= 0x1F;
		else
		errmsg = 3;

		//get out of shutdown mode to active state
		MSB |= 0x10;
		SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
		//now write to DAC
		// take the CS pin low to select the chip:
		digitalWrite(DAC_CS_pin,LOW);
		delay(1);
		//  send in the address and value via SPI:
		SPI.transfer(MSB);
		SPI.transfer(LSB);
		delay(1);
		// take the CS pin high to de-select the chip:
		digitalWrite(DAC_CS_pin,HIGH);
	 	SPI.endTransaction();

/*	 	byte mask;
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

	 	Serial.println();*/
	}
}

void MCP4822::gainSet(char new_gain)
{
  gain = new_gain;
}

unsigned int MCP4822::readErrmsg(void)
{
	return errmsg;
}
