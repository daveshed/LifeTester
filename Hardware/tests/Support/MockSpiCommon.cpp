// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

#include <string.h>         // memset
#include "SpiCommon.h"
#include "MockSpiCommon.h"  // declaration of mockSpiState and helper functions

MockSpiState_t mockSpiState;
/*
 Callback which gives us access to spi bus from tests ie. when source calls
 SpiTransferByte, it actually calls a function implemented in the test code.
 */
uint8_t (*SpiTransferByte_Callback)(uint8_t);
void (*OpenSpiConnection_Callback)(const SpiSettings_t*);
void (*CloseSpiConnection_Callback)(const SpiSettings_t*);

// Initialisation only
void InitChipSelectPin(uint8_t pin)
{
    // mock function behaviour
    mock().actualCall("InitChipSelectPin")
        .withParameter("pin", pin);
    
    // update spi settings from module under test
    mockSpiState.settings->chipSelectPin = pin;
    mockSpiState.initialised = true;
}

// Function to open SPI connection on required CS pin
void OpenSpiConnection(const SpiSettings_t *settings)
{
    // Can't open a connection unless bus is initialised - CS pin needs to be set
    CHECK(mockSpiState.initialised);

    // mock function behaviour
    mock().actualCall("OpenSpiConnection")
        .withParameter("settings", (void *)settings);
    
    mockSpiState.connectionOpen = true;

    // Callback function accessible from source via function pointer.
    OpenSpiConnection_Callback(settings);
}

// Transmits and receives a byte at the same time.
uint8_t SpiTransferByte(const uint8_t byteToSpiBus)
{
    // Can't transfer data unless the connection to the bus is open
    CHECK(mockSpiState.connectionOpen);

    // mock function behaviour
    mock().actualCall("SpiTransferByte")
        .withParameter("byteToSpiBus", byteToSpiBus);

    // Callback function accessible from source via function pointer.
    return SpiTransferByte_Callback(byteToSpiBus);
}

void CloseSpiConnection(const SpiSettings_t *settings)
{
    // mock function behaviour
    mock().actualCall("CloseSpiConnection")
        .withParameter("settings", (void *)settings);
    mockSpiState.connectionOpen = false;

    // Callback function accessible from source via function pointer.
    CloseSpiConnection_Callback(settings);
}

void InitialiseMockSpiBus(SpiSettings_t *settings)
{
    // sets data to 0 and bools to false
    memset(&mockSpiState, 0U, (sizeof(MockSpiState_t) - sizeof(SpiSettings_t)));
    // Point the mock spi state variable to settings from module under test
    mockSpiState.settings = settings;
    /* set chipSelectPin only. Other members contain settings that are only
    written once by the module under test. */
    mockSpiState.settings->chipSelectPin = 0xFF;
}

void SetSpiReadReg(uint16_t data)
{
    // data in the spi read reg can only be updated when connection is closed.
    CHECK(!mockSpiState.connectionOpen);
    
    const uint8_t msb = (data >> 8U) & 0xFF;
    mockSpiState.readReg.msb = msb;
    
    const uint8_t lsb = data & 0xFF;
    mockSpiState.readReg.lsb = lsb;
    
    // Reset the transfer index
    mockSpiState.transferIdx = 0U;
}

uint16_t GetSpiReadReg(void)
{
    return (uint16_t)((mockSpiState.readReg.msb << 8U) | mockSpiState.readReg.lsb);
}
