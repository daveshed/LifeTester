#ifndef CONFIG_H
#define CONFIG_H
#include "LifeTesterTypes.h"

#define DEBUG 1 //1 for serial communication, 0 to turn off.

///////////////////
//Board Revisions//
///////////////////
#define BOARD_REV_1     //Revision number of Lifetester PCB

//Hardware specific settings
#if defined(BOARD_REV_0)
  #define ADC_A_CS_PIN      8u     //ADC_A chip select. B is on pin8 
  #define ADC_B_CS_PIN      9u     //ADC_A chip select. B is on pin8 
  #define DAC_CS_PIN        7u     //DAC chip select 
  #define LED_A_PIN         6u     //LED indicator
  #define LED_B_PIN         5u     //LED indicator
  #define TEMP_CS_PIN       10u    //chip select for the MAX6675
  #define LIGHT_SENSOR_PIN  0u     //light intensity on A0
  #define ADC_CS_DELAY      200u   //delay time between CS low/high and data transfer
  #define MAX_CURRENT       4095u  //maximum reading allowed by ADC
#elif defined(BOARD_REV_1)
  #define ADC_CS_PIN        9u
  #define DAC_CS_PIN        10u 
  #define LED_A_PIN         2u
  #define LED_B_PIN         4u
  #define TEMP_CS_PIN       7u
  #define LIGHT_SENSOR_PIN  0u
  #define MAX_CURRENT       65535u
#else
  #error "Board not defined."
#endif

#define I2C_ADDRESS     10    //address for this I2C slave device
#define CS_DELAY        100u  //delay between CS edge and spi transfer in ms

////////////////////////
//Measurement settings//
////////////////////////
/*
 * Note that MPPT step size needs to be large enough such that there is a noticeable
 * change in power between points so that the perturb-observe algorithm will see it and 
 * adjust to the point with increased power.
 */
#define MAX_ERROR_READS   20u  //number of allowed bad readings before error state
#define V_SCAN_MIN        0u
#define V_SCAN_MAX        100u
#define DV_SCAN           1u   //step size in MPP scan
#define DV_MPPT           1u
#define I_THRESHOLD       50u  //required threshold ADCreading in MPPscan for test to start
#define SETTLE_TIME       200u //settle time after setting DAC to ADC measurement
#define SAMPLING_TIME     200u //time interval over which ADC measurements are made continuously then averaged afterward
#define TRACK_DELAY_TIME  200u //time period between tracking measurements

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
