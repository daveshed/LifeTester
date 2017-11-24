#ifndef LIFETESTERTYPES_H
#define LIFETESTERTYPES_H

#ifdef _cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "LedFlash.h"

typedef enum {
  ok,
  low_current,
  current_limit,
  threshold,
  DAC_error
} errorCode;

typedef struct IVData_s {
  uint32_t v;
  uint32_t pCurrent;
  uint32_t pNext;
  uint32_t iCurrent;
  uint32_t iNext;
  uint32_t iTransmit;
} IVData_t;

typedef struct LifeTester_s {
  char          channel;
  Flasher       Led;
  uint16_t      nReadsCurrent;
  uint16_t      nReadsNext;   //counting number of readings taken by ADC during sampling window
  uint16_t      nErrorReads;  //number of readings outside allowed limits
  errorCode     error;          
  IVData_t      IVData;       //holds operating points
  uint32_t      timer;        //timer for tracking loop
} LifeTester_t;

#ifdef _cplusplus
}
#endif

#endif //include guard
