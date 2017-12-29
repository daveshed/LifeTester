#include "SpiCommon.h" // SpiSettings_t
#include "SpiConfig.h" // spi #defines

#define SPI_CLOCK_SPEED                 (5000000)     
#define SPI_BIT_ORDER                   (MSBFIRST)
#define SPI_DATA_MODE                   (SPI_MODE3)

#define TIMEOUT_MS                      (1000U)   
#define PWMout                          (3u)      //pin to output clock timer to ADC (pin 3 is actually pin 5 on ATMEGA328)

//MX7705 commands - note these are not very portable.
//TODO: rewrite binary commands in hex for portability
#define MX7705_REQUEST_CLOCK_WRITE      (B00100000)
#define MX7705_WRITE_CLOCK_SETTINGS     (B10010001)

#define MX7705_REQUEST_SETUP_READ_CH0   (B00011000)
#define MX7705_REQUEST_SETUP_READ_CH1   (B00011001)
#define MX7705_REQUEST_SETUP_WRITE_CH0  (B00010000)
#define MX7705_REQUEST_SETUP_WRITE_CH1  (B00010001)
#define MX7705_WRITE_SETUP_INIT         (B01000100)

#define MX7705_REQUEST_COMMS_READ_CH0   (B00001000)
#define MX7705_REQUEST_COMMS_READ_CH1   (B00001001)

#define MX7705_REQUEST_DATA_READ_CH0    (B00111000)
#define MX7705_REQUEST_DATA_READ_CH1    (B00111001)

extern SpiSettings_t mx7705SpiSettings;