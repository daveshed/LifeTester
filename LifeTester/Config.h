#ifndef CONFIG_H //guards to prevent header being included twice. Don't remove
#define CONFIG_H

#define DEBUG 1 //1 for serial communication, 0 to turn off.

////////////////
//pin settings//
////////////////
#define ADC_A_CS_PIN    8   //ADC_A chip select. B is on pin8 
#define ADC_B_CS_PIN    9   //ADC_A chip select. B is on pin8 
#define DAC_CS_PIN      7   //DAC chip select 
#define LED_A_PIN       6   //LED indicator
#define LED_B_PIN       5   //LED indicator
#define MAX_CS_PIN      10  //chip select for the MAX6675
#define LDR_PIN         0   //light intensity on A0
////////////////

#define I2C_ADDRESS     10  //address for this I2C slave device

////////////////////////
//Measurement settings//
////////////////////////
/*
 * Note that MPPT step size needs to be large enough such that there is a noticeable
 * change in power between points so that the perturb-observe algorithm will see it and 
 * adjust to the point with increased power.
 */
#define MAX_ERROR_READS   20  //number of allowed bad readings before error state
#define V_SCAN_MIN        0
#define V_SCAN_MAX        100
#define DV_SCAN           1   //step size in MPP scan
#define DV_MPPT           1
#define I_THRESHOLD       50  //required threshold ADCreading in MPPscan for test to start
#define SETTLE_TIME       200 //settle time after setting DAC to ADC measurement
#define SAMPLING_TIME     200 //time interval over which ADC measurements are made continuously then averaged afterward
#define TRACK_DELAY_TIME  200 //time period between tracking measurements

///////////////////
//class instances//
///////////////////
MAX6675 TSense(MAX_CS_PIN);

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

//////////////////////////////////
//Initialise lifetester channels//
////////////////////////////////// 
LifeTester_t LTChannelA = {
  'a',
  Flasher(LED_A_PIN),
  ADS1286(ADC_A_CS_PIN),
  0,
  0,
  0,
  ok,
  {0},
  0
};

LifeTester_t LTChannelB = {
  'b',
  Flasher(LED_B_PIN),
  ADS1286(ADC_B_CS_PIN),
  0,
  0,
  0,
  ok,
  {0},
  0
};
#endif
