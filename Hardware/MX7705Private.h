#include "SpiCommon.h" // SpiSettings_t
#include "SpiConfig.h" // spi #defines

#define SPI_CLOCK_SPEED                 (5000000)     
#define SPI_BIT_ORDER                   (MSBFIRST)
#define SPI_DATA_MODE                   (SPI_MODE3)

#define TIMEOUT_MS                      (1000U)   
#define PWMout                          (3u)      //pin to output clock timer to ADC (pin 3 is actually pin 5 on ATMEGA328)

// Comms reg
#define REG_SELECT_OFFSET       (4U)
#define REG_SELECT_MASK         (7U)
#define CH0_BIT                 (0U)
#define CH1_BIT                 (1U)
#define CH_SELECT_MASK          (3U)
#define RW_SELECT_OFFSET        (3U)

// Setup Reg
#define FSYNC_BIT               (0U)
#define BUFFER_BIT              (1U)
#define BIPOLAR_UNIPOLAR_BIT    (2U)
#define PGA_OFFSET              (3U)
#define PGA_MASK                (7U)
#define MODE_OFFSET             (6U)
#define MODE_MASK               (3U)

// Clock reg - note bits 5-7 are reserved/read-only
#define FS0_BIT                 (0U)
#define FS1_BIT                 (1U)
#define CLK_BIT                 (2U)
#define CLK_DIV                 (3U)
#define CLK_DIS                 (4U)


//MX7705 commands - note these are not very portable.
//TODO: rewrite binary commands in hex for portability
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

extern SpiSettings_t mx7705SpiSettings;