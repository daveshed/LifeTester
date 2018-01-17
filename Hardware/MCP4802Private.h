#ifndef MCP4802PRIVATE_H
#define MCP4802PRIVATE_H

#include "Config.h"
#include "Macros.h"
#include "SpiCommon.h"
#include "SpiConfig.h"

#define SPI_CLOCK_SPEED  (20000000)  
#define SPI_BIT_ORDER    (MSBFIRST)
#define SPI_DATA_MODE    (SPI_MODE0)

#define DATA_MASK        (0xFF)     // data mask. chip dependent
#define DATA_OFFSET      (4U)       // offset to the dac data bits

#define CH_SELECT_BIT    (15U)
#define GAIN_SELECT_BIT  (13U)
#define SHDN_BIT         (12U)

// prototype gives access to this static function from tests
STATIC uint16_t MCP4802_GetDacCommand(chSelect_t   ch,
                                      gainSelect_t gain,
                                      shdnSelect_t shdn,
                                      uint8_t      output);

// Make spi settings available to test code.
extern SpiSettings_t MCP4802SpiSettings;
#endif //include guard