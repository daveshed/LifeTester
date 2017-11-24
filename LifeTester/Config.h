#ifndef CONFIG_H
#define CONFIG_H

#ifdef _cplusplus
extern "C" {
#endif 

#define DEBUG 0

#define ADC_CS_PIN        9u
#define DAC_CS_PIN        10u
#define LED_A_PIN         2u
#define LED_B_PIN         4u
#define TEMP_CS_PIN       7u
#define LIGHT_SENSOR_PIN  0u
#define MAX_CURRENT       65535u

#define I2C_ADDRESS     10    //address for this I2C slave device
#define CS_DELAY        10u  //delay between CS edge and spi transfer in ms

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

#endif
#ifdef _cplusplus
}
#endif