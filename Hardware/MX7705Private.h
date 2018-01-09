#include "Macros.h"
#include "SpiCommon.h" // SpiSettings_t
#include "SpiConfig.h" // spi #defines

#define SPI_CLOCK_SPEED         (5000000)     
#define SPI_BIT_ORDER           (MSBFIRST)
#define SPI_DATA_MODE           (SPI_MODE3)

#define TIMEOUT_MS              (1000U)   
#define PWMout                  (3u)      //pin to output clock timer to ADC (pin 3 is actually pin 5 on ATMEGA328)

// Comms reg
#define REG_SELECT_OFFSET       (4U)
#define REG_SELECT_MASK         (7U)
#define CH0_BIT                 (0U)
#define CH1_BIT                 (1U)
#define CH_SELECT_MASK          (3U)
#define CH_SELECT_OFFSET        (0U)
#define RW_BIT                  (3U)
#define DRDY_BIT                (7U)

// Setup Reg
#define FSYNC_BIT               (0U)
#define BUFFER_BIT              (1U)
#define BIPOLAR_UNIPOLAR_BIT    (2U)
#define PGA_OFFSET              (3U)
#define PGA_MASK                (7U)
#define MODE_OFFSET             (6U)
#define MODE_MASK               (3U)

// Clock reg - note bits 5-7 are reserved/read-only
#define FILTER_SELECT_MASK      (3U)
#define FILTER_SELECT_OFFSET    (0U)
#define CLK_BIT                 (2U)
#define CLKDIV_BIT              (3U)
#define CLKDIS_BIT              (4U)
#define MXID_BIT                (7U) // maxim id bit. Read only


//MX7705 commands - note these are not very portable.
//TODO: rewrite binary commands in hex for portability
#define MX7705_REQUEST_CLOCK_READ_CH0   (B00101000)
#define MX7705_REQUEST_CLOCK_READ_CH1   (B00101001)
#define MX7705_REQUEST_CLOCK_WRITE_CH0  (B00100000)
#define MX7705_REQUEST_CLOCK_WRITE_CH1  (B00100001)
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

/******************************************************************************
 * Private typedefs                                                           *
 ******************************************************************************/
// Adc mode setting applied to the setup register.
typedef enum AdcMode_e {
    NormalMode,
    SelfCalibMode,
    ZeroScaleCalibMode,
    FullScaleCalibMode,
    NumModes
} AdcMode_t;

static void MX7705_Write(uint8_t sendByte);
static uint8_t MX7705_ReadByte(void);
static uint16_t MX7705_Read16Bit(void);
#ifndef UNIT_TEST
    static void InitClockOuput(uint8_t pin); 
#endif
STATIC uint8_t RequestRegCommand(RegisterSelection_t demandedReg,
                                 uint8_t             channel,
                                 bool                readOp);
STATIC uint8_t RequestRegRead(RegisterSelection_t demandedReg, uint8_t channel);
STATIC uint8_t RequestRegWrite(RegisterSelection_t demandedReg, uint8_t channel);
STATIC uint8_t SetClockSettings(bool    clockOutDisable,
                                bool    internalClockDivOn,
                                bool    lowFreqClkIn,
                                uint8_t filterSelection);
STATIC uint8_t SetSetupSettings(AdcMode_t mode,
                                uint8_t pgaGainIdx,
                                bool unipolarMode,
                                bool enableBuffer,
                                bool filterSync);
static bool IsCommsRegBusy(uint8_t channel);


extern SpiSettings_t mx7705SpiSettings;