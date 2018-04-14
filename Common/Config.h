#ifndef CONFIG_H
#define CONFIG_H

#ifdef _cplusplus
extern "C" {
#endif 

#define ADC_CS_PIN          (10U)
#define DAC_CS_PIN          (9U)
#define LED_A_PIN           (5U)
#define LED_B_PIN           (6U)
#define COMMS_LED_PIN       (7U)  // TODO: check me
#define TEMP_CS_PIN         (8U)
#define LIGHT_SENSOR_PIN    (0U)

#define CS_DELAY            (1U)   // delay between CS edge and spi transfer in ms
#define CS_DELAY_ADC        (20U)  // ADC is particularly slow

//Measurement settings//
/*
 * Note that MPPT step size needs to be large enough such that there is a noticeable
 * change in power between points so that the perturb-observe algorithm will see it and 
 * adjust to the point with increased power.
 */
#define V_SCAN_MIN          (0U)
#define V_SCAN_MAX          (100U)
#define DV_SCAN             (1U)   //step size in MPP scan
#define DV_MPPT             (1U)
#define SETTLE_TIME         (200U) //settle time after setting DAC to ADC measurement
#define SAMPLING_TIME       (200U) //time interval over which ADC measurements are made continuously then averaged afterward
#define TRACK_DELAY_TIME    (200U) //time period between tracking measurements

// error handling
#define MAX_ERROR_READS     (20U)  //number of allowed bad readings before error state
#define MAX_CURRENT         (0xFFFFU)
#define MIN_CURRENT         (200U) // minimum current allowed during mpp update
#define THRESHOLD_CURRENT   (1000U) //required threshold ADCreading in MPPscan for test to start

// Led flasher timings
#define SCAN_LED_ON_TIME    (50U)
#define SCAN_LED_OFF_TIME   (500U)
#define ERROR_LED_ON_TIME   (500U)
#define ERROR_LED_OFF_TIME  (500U)
#define INIT_LED_ON_TIME    (100U)
#define INIT_LED_OFF_TIME   (100U)

void Config_SetSettleTime(uint16_t tSettle);
void Config_SetTrackDelay(uint16_t tDelay);
void Config_SetSampleTime(uint16_t tSample);
void Config_SetThresholdCurrent(uint16_t iThreshold);
uint16_t Config_GetSettleTime(void);
uint16_t Config_GetTrackDelay(void);
uint16_t Config_GetSampleTime(void);
uint16_t Config_GetThresholdCurrent(void);

#endif
#ifdef _cplusplus
}
#endif