// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "TC77.h"
#include "TC77Private.h"   // Spi settings

// support
#include "Arduino.h"       // arduino function prototypes - implemented MockAduino.c
#include "MockArduino.h"   // mockDigitalPins, mockMillis, private variables
#include "MockSpiCommon.h" // Mock spi interface
#include "SpiCommon.h"     // spi function prototypes - mocks implemented here.
#include <stdint.h>

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/

/* Return data depending on how many bytes we've already read. First MSB then
 LSB then nothing. Similar for writing data too. */
static uint8_t TransferDataFromReadReg(const uint8_t transmit)
{
    (void)transmit; // unused variable
    
    uint8_t retVal;
    if (mockSpiState.transferIdx == 0U)
    {
        // first send msb
        retVal = mockSpiState.readReg.msb;

        // now write data
        // mockSpiState.writeReg.msb = byteToSpiBus;

        mockSpiState.transferIdx++;
    }
    else if (mockSpiState.transferIdx == 1U)
    {
        // send lsb next
        retVal = mockSpiState.readReg.lsb;
        
        // now write data
        // mockSpiState.writeReg.lsb = byteToSpiBus;
        
        mockSpiState.transferIdx++;
    }
    else
    {
        // now send/write nothing
        retVal = 0U;
    }
    return retVal;
}

static void DummyCallback(const SpiSettings_t*)
{
    // dummy function
}

// set the status of mock hardware to ready
static void SetReadRegReady(uint16_t *readReg)
{
    bitSet(*readReg, 2U);
}

// set the status of mock hardware to busy
static void SetReadRegNotReady(uint16_t *readReg)
{
    bitClear(*readReg, 2U);
}

/* Puts some data into the spi read register given by the temperature it's
exposed to. See datasheet for details on calculation. */
static void UpdateReadReg(float temperature, bool ready)
{
    // convert temp into binary
    const int16_t signedData = (int16_t)(temperature / CONVERSION_FACTOR);

    uint16_t rawData = (uint16_t)(signedData << 3U);
    if (ready)
    {
        SetReadRegReady(&rawData);
    }
    else
    {
        SetReadRegNotReady(&rawData);
    }
    // Put the data into the register
    SetSpiReadReg(rawData);
}

// Mocks needed for reading data in update function
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
 * Unit tests
 ******************************************************************************/
// Define a test group for TC77 - all tests share common setup/teardown
TEST_GROUP(TC77TestGroup)
{
    void setup(void)
    {
        // Set function pointer from MockSpiCommon.h
        SpiTransferByte_Callback = &TransferDataFromReadReg;
        OpenSpiConnection_Callback = &DummyCallback;
        CloseSpiConnection_Callback = &DummyCallback;

        // Clear mock spi data
        InitialiseMockSpiBus(&tc77SpiSettings);
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
    UpdateReadReg(mockTemperature, false);
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
    UpdateReadReg(mockTemperature, true);
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;
    
    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    
    TC77_Update();

    // Check data returned against expectations
    const uint16_t rawDataActual = TC77_GetRawData();
    const uint16_t rawDataExpected = GetSpiReadReg();
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
    UpdateReadReg(mockTemperature, true);
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;

    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    
    // now call the update function
    TC77_Update();

    // Compare returned data against expectations
    const uint16_t rawDataExpected = GetSpiReadReg();
    const uint16_t rawDataActual = TC77_GetRawData();
    CHECK_EQUAL(rawDataExpected, rawDataActual);
    DOUBLES_EQUAL(mockTemperature, TC77_ConvertToTemp(rawDataActual), 0.1);
    
    // expect error condition because of over temperature
    CHECK(TC77_GetError());

    // Checking mock function calls
    mock().checkExpectations();
}