#ifndef LIFETESTERTYPES_H
#define LIFETESTERTYPES_H

#ifdef _cplusplus
extern "C"
{
#endif

#include "MCP4802.h"  // dac types
#include "LedFlash.h"
#include <stdint.h>

/*
 Table of error types in the lifetester system.
*/
typedef enum errorCode_e {
    ok,               // everything ok
    lowCurrent,       // low current error
    currentLimit,     // reached current limit during scan
    currentThreshold, // currrent threshold required for measurement not reached
    invalidScan       // scan is the wrong shape
} errorCode_t;

/*
 Measurements that the lifetester may take on a given channel. Note that this
 and next refer to the current operating point and another neigbouring point for
 comparison in mpp tracking.
*/
typedef struct LifeTesterData_s {
    uint32_t vThis;       // voltage of operating point (dac code)
    uint32_t vNext;       // voltage of the neighbouring point
    uint32_t vScan;       // voltage of point being scanned
    uint32_t vScanMpp;    // max power point measured in scan
    
    uint32_t pThis;       // power at this point
    uint32_t pNext;       // power at neighbouring point
    uint32_t pScan;       // power at point being scanned
    uint32_t pScanInitial;// power associated with first point
    uint32_t pScanFinal;  // ...and the last one (for checking scan shape)
    uint32_t pScanMpp;    // max power measured from scan

    uint32_t iThis;       // average current from current samples at this point
    uint32_t iNext;
    uint32_t iScan;
    uint32_t iScanMpp;
    uint32_t iSampleSum;  // sum of all currents measured during sampling window
    
    uint16_t nSamples;    // counting number of readings taken by ADC during sampling window
    uint16_t nErrorReads; // number of readings outside allowed limits
} LifeTesterData_t;

// holds the channel info for the DAC and ADC
typedef struct LifeTesterIo_s {
    const chSelect_t dac;
    const uint8_t    adc;
} LifeTesterIo_t;

/*
 Signature of lifetester state functions. They take a pointer to the lifetester
 object but don't return anything. In this object will be data corresponding to
 measurements and a pointer to the next state that needs to run.
*/
struct LifeTester_s;
typedef void StateFn_t(struct LifeTester_s *const);

/*
 Combined lifetester state and data type.
*/
struct LifeTester_s {
    LifeTesterIo_t    io;
    Flasher           led;
    LifeTesterData_t  data;
    StateFn_t         *nextState;
    uint32_t          timer;     //timer for tracking loop
    errorCode_t       error;          
};
typedef LifeTester_s LifeTester_t;

#ifdef _cplusplus
}
#endif

#endif //include guard
