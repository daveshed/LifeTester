#include "Config.h"    // CS_DELAY
#include "SpiCommon.h" // SpiSettings_t
#include "SpiConfig.h" // spi #defines

#define CONVERSION_TIME     (400U)      //measurement conversion time (ms) - places upper limit on measurement rate
#define CONVERSION_FACTOR   (0.0625f)   //deg C per code
#define MSB_OVERTEMP        (B00111110) //reading this on MSB implies overtemperature
#define SPI_CLOCK_SPEED     (7000000U)
#define SPI_BIT_ORDER       (MSBFIRST)
#define SPI_DATA_MODE       (SPI_MODE0)

#ifdef UNIT_TEST
    #define STATIC
#else
    #define STATIC static
#endif  // UNIT_TEST

extern STATIC SpiSettings_t tc77SpiSettings;  // TODO: is extern needed?
