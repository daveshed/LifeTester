#ifndef MCP48X2PRIVATE_H
#define MCP48X2PRIVATE_H

#include "Config.h"
#include "Macros.h"
#include "SpiCommon.h"
#include "SpiConfig.h"

#define SPI_CLOCK_SPEED  (20000000)  
#define SPI_BIT_ORDER    (MSBFIRST)
#define SPI_DATA_MODE    (SPI_MODE0)

#ifdef MCP4802
    #define DATA_MASK        (0xFF)     // data mask. chip dependent
    #define DATA_OFFSET      (4U)       // offset to the dac data bits
#elif defined (MCP4812)
    #define DATA_MASK        (0x400)
    #define DATA_OFFSET      (2U)
#elif defined (MCP4822)
    #define DATA_MASK        (0x1000)
    #define DATA_OFFSET      (0U)
#else
    #error "dac model not defined"
#endif

#define CH_SELECT_BIT    (15U)
#define GAIN_SELECT_BIT  (13U)
#define SHDN_BIT         (12U)

// prototype gives access to this static function from tests
STATIC uint16_t MCP48X2_GetDacCommand(chSelect_t  ch,
                                      gainSelect_t gain,
                                      shdnSelect_t shdn,
                                      uint16_t     output);

// Make spi settings available to test code.
extern SpiSettings_t mcp48x2SpiSettings;
#endif //include guard