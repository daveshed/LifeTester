#include "Macros.h"
#include "SpiCommon.h"
#include <stdint.h>
#include <stdbool.h>

#define CS_PIN_INIT         (0xFF)

// 16 bit register reprisenting data stored in spi bus.
typedef struct dataRegister_s {
    uint8_t msb;
    uint8_t lsb;
} dataRegister_t;

// struct to hold information on the state of the mock spi bus
typedef struct MockSpiState_s {
    bool           initialised;    // spi connection initialised
    bool           connectionOpen; // spi connection open/closed
    dataRegister_t readReg;       // read register of spi bus
    dataRegister_t writeReg;      // write register
    uint8_t        transferIdx;    // index of byte being transferred
    SpiSettings_t  *settings;      // pointer to spi settings stored in module under test
} MockSpiState_t;

extern MockSpiState_t mockSpiState;
extern uint8_t (*SpiTransferByte_Callback)(uint8_t);
extern void (*OpenSpiConnection_Callback)(const SpiSettings_t*);
extern void (*CloseSpiConnection_Callback)(const SpiSettings_t*);


// Reset all data in the mock spi bus excluding the data pointed at in settings
void InitialiseMockSpiBus(SpiSettings_t *settings);

// write a uint16_t into the spi read register.
void SetSpiReadReg(uint16_t data);

// Gets the contents of the mock spi read register
uint16_t GetSpiReadReg(void);