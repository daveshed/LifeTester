// ensure this library description is only included once
#ifndef MCP4822_h
#define MCP4822_h

#include "Arduino.h"

class MCP4822
{
	public:
		MCP4822(uint8_t); //initialise
		void output(char, uint16_t);
  	void gainSet(char);
  	uint8_t readErrmsg(void);

	private:
		// Class Member Variables
	  	uint8_t MSB, LSB;
	  	uint8_t gain;
	  	uint8_t errMsg; //pass error code to user
	  	uint8_t DacCsPin; //chip select pin
};

#endif
