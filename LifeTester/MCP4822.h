// ensure this library description is only included once
#ifndef MCP4822_h
#define MCP4822_h

#include "Arduino.h"

class MCP4822
{
	public:
		MCP4822(unsigned int); //initialise
		void output(char, unsigned int);
  		void gainSet(char);
  		unsigned int readErrmsg(void);

	private:
		// Class Member Variables
	  	byte MSB,LSB;
	  	unsigned char gain;
	  	unsigned int errmsg; //pass error code to user
	  	unsigned int DAC_CS_pin; //chip select pin
};

#endif