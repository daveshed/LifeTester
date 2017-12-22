// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

// Code under test
#include "TC77.h"

// support
#include "Arduino.h"
#include "MockArduino.h"
#include "SpiCommon.h"
#include <stdint.h>


/*******************************************************************************
 * Private data and defines
 ******************************************************************************/
/* none */
/*******************************************************************************
 * Private data types used in tests
 ******************************************************************************/
/* none */
/*******************************************************************************
 * Private test data
 ******************************************************************************/
// value to be returned by millis()
// static uint32_t mockMillis;

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
/* none */
/*******************************************************************************
 * Mock functions
 ******************************************************************************/
#if 1
// Initialisation only
void SpiInit(SpiSettings_t settings)
{
    #if 0
    // mock function behaviour
    mock().actualCall("SpiInit")
        .withParameter("settings", settings);
    #endif
}
// Function to open SPI connection on required CS pin
void OpenSpiConnection(SpiSettings_t settings)
{
    #if 0
    // mock function behaviour
    mock().actualCall("OpenSpiConnection")
        .withParameter("settings", settings);
    #endif
}

// Transmits and receives a byte
uint8_t SpiTransferByte(uint8_t transmit)
{
    #if 0
    // mock function behaviour
    mock().actualCall("SpiTransferByte")
        .withParameter("transmit", transmit);

    return 0U;
    #endif
}

// Function to close SPI connection on required CS pin
void CloseSpiConnection(SpiSettings_t settings)
{
    #if 0
    // mock function behaviour
    mock().actualCall("CloseSpiConnection")
        .withParameter("settings", settings);
    #endif
}
#endif

#if 0
long unsigned int millis(void)
{
    mock().actualCall("millis");
    return mockMillis;
}
#endif
/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group for TC77 - all tests share common setup/teardown
TEST_GROUP(TC77TestGroup)
{
    void setup(void)
    {
        mockMillis = 0U;
    }

    void teardown(void)
    {
        mock().clear();
    }
};

// Test for turning on the Led constantly
TEST(TC77TestGroup, dummy)
{
    FAIL("dummy test");
#if 0
    // Turn on the Led constantly
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", HIGH);
   
    mock().checkExpectations();
#endif
}