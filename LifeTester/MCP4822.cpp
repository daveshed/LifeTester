/*
helpful info on making a library herehttps://www.arduino.cc/en/Hacking/libraryTutorial

*/

#include "MCP4822.h"
#include "Arduino.h"
#include <SPI.h>

//initialise class member variables
uint8_t gain;
uint8_t errMsg;
uint8_t DacCsPin;

//constructors
//initialise with certain things when we create it
MCP4822::MCP4822(uint8_t pin)
{
	//assign chip select to pin output when the class is created.	
	DacCsPin = pin;
  errMsg = 0;
  pinMode(DacCsPin, OUTPUT);
  digitalWrite(DacCsPin,HIGH);
  SPI.begin();
}

void MCP4822::output(char channel, unsigned int output)
{
	uint8_t MSB,LSB;
	errMsg = 0;
	//only run the rest of the code if request is in range.
	if (output < 0 || output > 4095)
  {
		errMsg = 1;
  }
	else
	{
		//convert decimal output to binary stored in two bytes
		MSB = output >> 8;    //most sig byte
		LSB = output;         //least sig byte

		//apply config bits to the front of MSB
		if (channel=='a' || channel=='A')
		{
		  MSB &= 0x7F; //writing a 0 to bit 7.
		}
		else if (channel=='b' || channel=='B')
		{
		  MSB |= 0x80; //writing a 1 to bit 7.
		}
		else
		{
		  errMsg = 2;
		}
		
		if (gain=='l' || gain=='L')
		{
		  MSB |= 0x20;
		}
		else if (gain=='h' || gain=='H')
		{
		  MSB &= 0x1F;
		}
		else
    {
      errMsg = 3;  
    }
		
		//get out of shutdown mode to active state
		MSB |= 0x10;
		SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
		//now write to DAC
		// take the CS pin low to select the chip:
		digitalWrite(DacCsPin,LOW);
		delay(1);
		//  send in the address and value via SPI:
		SPI.transfer(MSB);
		SPI.transfer(LSB);
		delay(1);
		// take the CS pin high to de-select the chip:
		digitalWrite(DacCsPin,HIGH);
	 	SPI.endTransaction();
	}
}

void MCP4822::gainSet(char new_gain)
{
  gain = new_gain;
}

uint8_t MCP4822::readErrmsg(void)
{
	return errMsg;
}
