// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "TC77.h"
#include "TC77Private.h"

// support
#include "Arduino.h"     // arduino function prototypes - implemented MockAduino.c
#include "MockArduino.h" // mockDigitalPins, mockMillis, private variables
#include "SpiCommon.h"   // spi function prototypes - mocks implemented here.
#include <stdint.h>
#include "TestUtils.h"   // digital pin mocking

#include <stdio.h>

/*******************************************************************************
 * Private data types used in tests
 ******************************************************************************/
// struct to hold information on the state of the mock spi bus
typedef struct MockSpiState_s {
    bool          initialised;      // spi connection initialised
    bool          connectionOpen;   // spi connection open/closed
    uint8_t       readRegMsb;       // msb stored in read bus
    uint8_t       readRegLsb;       // lsb stored in read bus
    uint8_t       readIdx;
    SpiSettings_t settings;
} MockSpiState_t;
/*******************************************************************************
 * Private data and defines
 ******************************************************************************/
// Spi hardware state variables held in struct
MockSpiState_t mockSpiState;
/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
static void InitialiseMockSpiBus(void)
{
    // sets data to 0 and bools to false
    memset(&mockSpiState, 0U, (sizeof(MockSpiState_t) - sizeof(SpiSettings_t)));
    /* set chipSelectPin only. Other members contain settings that are only
    written once. */
    mockSpiState.settings.chipSelectPin = 0xFF;
}

// write a uint16_t into the spi read register.
static void SetSpiReadReg(uint16_t data)
{
    const uint8_t lsb = data & 0xFF;
    const uint8_t msb = (data >> 8U) & 0xFF;
    mockSpiState.readRegMsb = msb;
    mockSpiState.readRegLsb = lsb;
    mockSpiState.readIdx = 0U;
}

static void SetTC77RegReady(uint16_t *readReg)
{
    bitSet(*readReg, 2U);
}

static void SetTC77RegNotReady(uint16_t *readReg)
{
    bitClear(*readReg, 2U);
}

/* Puts some data into the spi read register given by the temperature it's
exposed to. See datasheet for details on calculation. */
static void UpdateTC77ReadReg(float temperature, bool ready)
{
    // data in the spi read reg can only be updated when connection is closed.
    CHECK(!mockSpiState.connectionOpen);
    // convert temp into binary
    const int16_t signedData = (int16_t)(temperature / CONVERSION_FACTOR);

    uint16_t rawData = (uint16_t)(signedData << 3U);
    if (ready)
    {
        SetTC77RegReady(&rawData);
    }
    else
    {
        SetTC77RegNotReady(&rawData);
    }
    // Put the data into the register
    SetSpiReadReg(rawData);
}

// Gets the contents of the mock spi read register
static uint16_t GetTC77RawDataExpected(void)
{
    return (uint16_t)((mockSpiState.readRegMsb << 8U) | mockSpiState.readRegLsb);
}

static void MockForTC77ReadRawData(void)
{
    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &tc77SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &tc77SpiSettings);
}
/*******************************************************************************
 * Mock functions
 ******************************************************************************/
// Initialisation only
void InitChipSelectPin(uint8_t pin)
{
    // mock function behaviour
    mock().actualCall("InitChipSelectPin")
        .withParameter("pin", pin);
    
    // update spi settings from module under test
    tc77SpiSettings.chipSelectPin = pin;
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
}

// Transmits and receives a byte - only returns data for now.
uint8_t SpiTransferByte(const uint8_t byteToSpiBus)
{
    // Can't transfer data unless the connection to the bus is open
    CHECK(mockSpiState.connectionOpen);

    // mock function behaviour
    mock().actualCall("SpiTransferByte")
        .withParameter("byteToSpiBus", byteToSpiBus);

    /* Return data depending on how many bytes we've already read. First MSB then
    LSB then nothing. */
    uint8_t retVal;
    if (mockSpiState.readIdx == 0U)
    {
        // first send msb
        retVal = mockSpiState.readRegMsb;
        mockSpiState.readIdx++;
    }
    else if (mockSpiState.readIdx == 1U)
    {
        // send lsb next
        retVal = mockSpiState.readRegLsb;
        mockSpiState.readIdx++;
    }
    else
    {
        // now send nothing
        retVal = 0U;
    }
    return retVal;
}

// Function to close SPI connection on required CS pin
void CloseSpiConnection(const SpiSettings_t *settings)
{
    // mock function behaviour
    mock().actualCall("CloseSpiConnection")
        .withParameter("settings", (void *)settings);
    mockSpiState.connectionOpen = false;
}

/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group for TC77 - all tests share common setup/teardown
TEST_GROUP(TC77TestGroup)
{
    void setup(void)
    {
        InitialiseMockSpiBus();
        mockMillis = 0U;
    }

    void teardown(void)
    {
        mock().clear();
    }
};

// Test for initialising TC77 device.
TEST(TC77TestGroup, InitialiseTC77)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    CHECK_EQUAL(CS_DELAY, tc77SpiSettings.chipSelectDelay);
    CHECK_EQUAL(SPI_CLOCK_SPEED, tc77SpiSettings.clockSpeed);
    CHECK_EQUAL(SPI_BIT_ORDER, tc77SpiSettings.bitOrder);
    CHECK_EQUAL(SPI_DATA_MODE, tc77SpiSettings.dataMode);

    // check function calls
    mock().checkExpectations();
}

/*
 Test for initialising then calling update function on TC77 device before 
 conversion has taken place. So 0 should be returned.
 */

TEST(TC77TestGroup, ReadingTC77BeforeConversionNoData)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    mock().expectOneCall("millis");
    TC77_Update();
    CHECK_EQUAL(0U, TC77_GetRawData());   
    mock().checkExpectations();
}

/*
 Test for initialising then calling update function on TC77 device before 
 the device is ready - reading the ready bit of the read register. Expect 0 to be
 returned even though there is data in the register ie. initial data doesn't get
 updated and 0 remains.
 */

TEST(TC77TestGroup, ReadingTC77AfterConversionNotReady)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    mock().expectOneCall("millis");
    
    // Put data into read register - do not expect to see this. Device not ready.
    const float mockTemperature = 25.2F;
    // Now update read reg but set state to busy
    UpdateTC77ReadReg(mockTemperature, false);
    TC77_Update();
    
    CHECK_EQUAL(0U, TC77_GetRawData());   
    mock().checkExpectations();
}


/*
 Test for calling update function on TC77 device after conversion time. We expect
 data to be available and for it to be returned over spi bus. 
 */
TEST(TC77TestGroup, ReadingTC77AfterConversionExpectData)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function for init
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    // Do not expect error condition
    CHECK(!TC77_GetError());

    // Put data into read register
    const float mockTemperature = 25.2F;
    UpdateTC77ReadReg(mockTemperature, true);
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;
    
    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    mock().expectOneCall("millis");
    
    TC77_Update();

    // Check data returned against expectations
    const uint16_t rawDataActual = TC77_GetRawData();
    const uint16_t rawDataExpected = GetTC77RawDataExpected();
    CHECK_EQUAL(rawDataExpected, rawDataActual);
    DOUBLES_EQUAL(mockTemperature, TC77_ConvertToTemp(rawDataActual), 0.1);
    // Do not expect error condition
    CHECK(!TC77_GetError());
    
    // Checking mock function calls
    mock().checkExpectations();
}

/*
 Test for calling update function on TC77 device after conversion time with an
 overtemp condition. Data should be returned but with error condition must be
 set to true.
 */
TEST(TC77TestGroup, ReadingTC77AfterConversionWithOverTemp)
{
    const uint8_t pinNum = 1U;
    
    // Mock calls to low level spi function for init
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    // Call init function with required pin setting - chip select
    TC77_Init(pinNum);
    // Should be no error condition at this point.
    CHECK(!TC77_GetError());

    /* max temperature is 125C. So setting the temperature above this should
    trigger an overtemperature error */
    const float mockTemperature = 128.4F;  // f for float not farenheit!  
    /* Now that the temperature is set, we can update the contents of the TC77's
    read register with a mock value that corresponds to it.*/
    UpdateTC77ReadReg(mockTemperature, true);
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;

    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    mock().expectOneCall("millis");
    
    // now call the update function
    TC77_Update();

    // Compare returned data against expectations
    const uint16_t rawDataExpected = GetTC77RawDataExpected();
    const uint16_t rawDataActual = TC77_GetRawData();
    CHECK_EQUAL(rawDataExpected, rawDataActual);
    DOUBLES_EQUAL(mockTemperature, TC77_ConvertToTemp(rawDataActual), 0.1);
    
    // expect error condition because of over temperature
    CHECK(TC77_GetError());

    // Checking mock function calls
    mock().checkExpectations();
}