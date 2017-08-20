#include <SPI.h>
#include <Wire.h>
#include "LEDFlash.h"
#include "ADS1286.h"
#include "MAX6675.h"

//1 for serial communication, 0 to turn off.
#define DEBUG 1

////////////////
//pin settings//
////////////////
const uint8_t AdcA_CSPin = 8;//ADC_A chip select. B is on pin8 
const uint8_t AdcB_CSPin = 9; //ADC_A chip select. B is on pin8 
const uint8_t Dac_CSPin  = 7; //DAC chip select 
const uint8_t LedA_Pin   = 6; //LED indicator
const uint8_t LedB_Pin   = 5; //LED indicator
const uint8_t Max_CSPin  = 10; //chip select for the MAX6675
const uint8_t LdrPin     = 0; //light intensity on A0

//address for this slave device
const uint8_t I2CAddress = 10;

///////////
//Globals//
///////////
const uint16_t tolerance = 20;  //number of allowed bad readings before there is an error
const uint16_t VScanMin = 0;
const uint16_t VScanMax = 100;
const uint16_t dVScan = 1;     //step size in MPP scan
const uint16_t dVMPPT = 1;      //step size during MPPT - note that if this is too small,
                                // there won't be a noticeable change in power between points and the alogorithm won't work.
const uint16_t iThreshold = 50; //required threshold ADCreading in MPPscan for test to start

//////////////////////
//measurement timing//
//////////////////////
const uint16_t settleTime     = 200; //settle time after setting DAC to ADC measurement
const uint16_t samplingTime   = 200; //time interval over which ADC measurements are made continuously then averaged afterward
const uint16_t trackingDelay  = 200; //time period between tracking measurements


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
  char          DacChannel;
  Flasher       Led;
  ADS1286       AdcInput;
  uint16_t      nReadsCurrent;
  uint16_t      nReadsNext;   //counting number of readings taken by ADC during sampling window
  uint16_t      nErrorReads;  //number of readings outside allowed limits
  errorCode     error;          
  IVData_t      IVData;       //holds operating points
  uint32_t      timer;        //timer for tracking loop
} LifeTester_t;

////////////////
//DAC settings//
////////////////
uint8_t     DacErrmsg;      //error code from DACset
const char  DacGain = 'l';  //gain setting for DAC

///////////////////
//class instances//
///////////////////
MAX6675 TSense(Max_CSPin);

//////////////////////////////////
//Initialise lifetester channels//
////////////////////////////////// 
LifeTester_t LTChannelA = {
  'a',
  Flasher(LedA_Pin),
  ADS1286(AdcA_CSPin),
  0,
  0,
  0,
  ok,
  {0},
  0
};

LifeTester_t LTChannelB = {
  'b',
  Flasher(LedB_Pin),
  ADS1286(AdcB_CSPin),
  0,
  0,
  0,
  ok,
  {0},
  0
};

