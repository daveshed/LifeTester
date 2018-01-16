#ifndef MCP48X2_H
#define MCP48X2_H

#ifdef _cplusplus
extern "C"
{
#endif

#include <stdint.h>

/*
 * library to control MCP48X2 DACs by microchip. Note that 12, 10, and 8 bit 
 * devices can be controlled with this library but that only 8 bit resolution is
 * used. The lifetester only requires this resolution which is < 0.01V.
 */

// gain selection type
typedef enum gainSelect_e {
    highGain,
    lowGain
} gainSelect_t;

// shutdown on/off type
typedef enum shdnSelect_e {
    shdnOn,
    shdnOff
} shdnSelect_t;

// selects chA/chB
typedef enum chSelect_e {
    chASelect,
    chBSelect
} chSelect_t;

// Function to initialise the interface with DAC
void MCP48X2_Init(uint8_t pin);

/*
 * Function to set the channel ('a' or 'b') to the required code
 * Note that the DAC expects a 16Bit write command but the 4 least sig
 * bits are ignored.
 * Bit 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *     A/B - GA SD D7 D6 D5 D4 D3 D2 D1 D0  x  x  x  x
 */
void MCP48X2_Output(uint8_t output, chSelect_t ch);

// Shut down the given channel
void MCP48X2_Shutdown(chSelect_t ch);

// Set the gain. This applies to both channels.
void MCP48X2_SetGain(gainSelect_t requestedGain);

// Gets the current gain setting.
gainSelect_t MCP48X2_GetGain(void);

#ifdef _cplusplus
}
#endif

#endif