#ifndef LIFETESTERTYPES_H
#define LIFETESTERTYPES_H

#ifdef _cplusplus
extern "C"
{
#endif

#include "MCP4802.h"  // dac types
#include "LedFlash.h"
#include <stdint.h>

typedef enum errorCode_e {
    ok,               // everything ok
    lowCurrent,       // low current error
    currentLimit,     // reached current limit during scan
    currentThreshold, // currrent threshold required for measurement not reached
    invalidScan       // scan is the wrong shape
} errorCode_t;

typedef struct IVData_s {
    uint32_t v;
    uint32_t pCurrent;
    uint32_t pNext;
    uint32_t iCurrent;
    uint32_t iNext;
    uint32_t iTransmit;
} IVData_t;

// holds the channel info for the DAC and ADC
typedef struct LtChannel_s {
    chSelect_t dac;
    uint8_t    adc;
} LtChannel_t;

typedef struct LifeTester_s {
    LtChannel_t channel;
    Flasher     Led;
    uint16_t    nReadsCurrent;
    uint16_t    nReadsNext;   //counting number of readings taken by ADC during sampling window
    uint16_t    nErrorReads;  //number of readings outside allowed limits
    errorCode_t error;          
    IVData_t    IVData;       //holds operating points
    uint32_t    timer;        //timer for tracking loop
} LifeTester_t;

#ifdef _cplusplus
}
#endif

#endif //include guard
