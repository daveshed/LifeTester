#ifndef CONFIG_H //guards to prevent header being included twice. Don't remove
#define CONFIG_H

#define DEBUG 1 //1 for serial communication, 0 to turn off.

///////////////////
//Board Revisions//
///////////////////
#define BOARD_REV_1     //Revision number of Lifetester PCB

//Hardward specific settings
#if defined(BOARD_REV_0)
  #define ADC_A_CS_PIN      8     //ADC_A chip select. B is on pin8 
  #define ADC_B_CS_PIN      9     //ADC_A chip select. B is on pin8 
  #define DAC_CS_PIN        7     //DAC chip select 
  #define LED_A_PIN         6     //LED indicator
  #define LED_B_PIN         5     //LED indicator
  #define TEMP_CS_PIN       10    //chip select for the MAX6675
  #define LIGHT_SENSOR_PIN  0     //light intensity on A0
  #define ADC_CS_DELAY      200   //delay time between CS low/high and data transfer
  #define MAX_CURRENT       4095  //maximum reading allowed by ADC
#elif defined(BOARD_REV_1)
  #define ADC_CS_PIN        9
  #define DAC_CS_PIN        10 
  #define LED_A_PIN         2
  #define LED_B_PIN         4
  #define TEMP_CS_PIN       7
  #define LIGHT_SENSOR_PIN  0
  #define MAX_CURRENT       65535
#else
  #error "Board not defined."
#endif

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

//////////////////////////////////
//Initialise lifetester channels//
////////////////////////////////// 
LifeTester_t LTChannelA = {
  'a',
  Flasher(LED_A_PIN),
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
  0,
  0,
  0,
  ok,
  {0},
  0
};
#endif
