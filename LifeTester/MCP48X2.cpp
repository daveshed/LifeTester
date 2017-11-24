#include "Config.h"
#include "MCP48X2.h"
#include "SPI.h" //for SPI #defines
#include "SpiCommon.h"

/*
 * library to control MCP48X2 DACs by microchip. Note that 12, 10, and 8 bit 
 * devices can be controlled with this library but that only 8 bit resolution is
 * used. The lifetester only requires this resolution which is < 0.01V.
 */

#define SPI_CLOCK_SPEED  20000000     
#define SPI_BIT_ORDER    MSBFIRST
#define SPI_DATA_MODE    SPI_MODE0
#define DEBUG            0

static char gain = 'l';
static uint8_t errMsg = 0;

/*
 * Function to initialise the interface with DAC
 */
void MCP48X2_Init(uint8_t chipSelectPin)
{
  errMsg = 0;
  SpiInit(chipSelectPin);
  
  MCP48X2_SetGain(gain);
  MCP48X2_Output(chipSelectPin, 0, 'a');
  MCP48X2_Output(chipSelectPin, 0, 'b');
}

/*
 * Function to set the channel ('a' or 'b') to the required code
 * Note that the DAC expects a 16Bit write command but the 4 least sig
 * bits are ignored.
 * Bit 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *     A/B - GA SD D7 D6 D5 D4 D3 D2 D1 D0  x  x  x  x
 */
void MCP48X2_Output(char chipSelectPin, uint8_t output, char channel)
{
  uint8_t   MSB, LSB;
  uint16_t  DacCommand = 0;

  //check that requested output code is within range
  if (output > 255)
  {
    errMsg = 1;
  }
  else
  {
    errMsg = 0;

    //1. write requested output into command
    DacCommand = (uint16_t)output << 4;
   
    //2. apply config bits to the front of MSB
    if (channel == 'a' || channel == 'A')
    {
      bitWrite(DacCommand, 15, 0);
    }
    else if (channel=='b' || channel=='B')
    {
      bitWrite(DacCommand, 15, 1);
    }
    else
    {
      errMsg = 2;
    }
    
    if (gain=='l' || gain=='L')
    {
      bitWrite(DacCommand, 13, 1);
    }
    else if (gain=='h' || gain=='H')
    {
      bitWrite(DacCommand, 13, 0);
    }
    else
    {
      errMsg = 3;
    }
    
    //get out of shutdown mode to active state
    if (!errMsg)
    {
      bitWrite(DacCommand, 12, 1);
    }
    
    
    //convert 16bit command to two 8bit bytes
    MSB = DacCommand >> 8;
    LSB = DacCommand;

    #if DEBUG
      Serial.print("MCP48X2 sending: ");
      Serial.print(MSB, BIN);
      Serial.print(" ");
      Serial.println(LSB, BIN);
    #endif
    
    //now write to DAC
    OpenSpiConnection(chipSelectPin, CS_DELAY, SPI_CLOCK_SPEED, SPI_BIT_ORDER, SPI_DATA_MODE);
    
    //  send in the address and value via SPI:
    SPI.transfer(MSB);
    SPI.transfer(LSB);
    
    CloseSpiConnection(chipSelectPin, CS_DELAY);
  }
}

void MCP48X2_SetGain(char requestedGain)
{
  gain = requestedGain;
}

char MCP48X2_GetGain(void)
{
  return gain;
}

uint8_t MCP48X2_GetErrmsg(void)
{
  return errMsg;
}
// undefine here so we don't end up with settings from another device
#undef SPI_CLOCK_SPEED
#undef SPI_BIT_ORDER
#undef SPI_DATA_MODE

