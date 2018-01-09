#include "Config.h"    // CS_DELAY
#include "SpiCommon.h" // SpiSettings_t
#include "SpiConfig.h" // spi #defines

#define CONVERSION_TIME     (400U)      //measurement conversion time (ms) - places upper limit on measurement rate
#define CONVERSION_FACTOR   (0.0625F)   //deg C per code
#define MSB_OVERTEMP        (B00111110) //reading this on MSB implies overtemperature
#define SPI_CLOCK_SPEED     (7000000U)
#define SPI_BIT_ORDER       (MSBFIRST)
#define SPI_DATA_MODE       (SPI_MODE0)

// extern required so we can access from tests
extern SpiSettings_t tc77SpiSettings;