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

#define SPI_DATA_FIFO_LENGTH   (2U)  // length of data stored in spi output buffer from device

/*******************************************************************************
 * Private data types used in tests
 ******************************************************************************/
#if 1
// struct to hold information on the state of the mock spi bus
typedef struct MockSpiState_s {
    bool          initialised;                   // spi connection initialised
    bool          connectionOpen;                // spi connection open/closed
    uint8_t       readReg[SPI_DATA_FIFO_LENGTH]; // bytes to read from bus
    uint8_t       readIdx;                       // keep count of where we are
    // uint8_t    writeReg;                   // bytes sent to bus
    SpiSettings_t settings;
} MockSpiState_t;
#endif
/*******************************************************************************
 * Private data and defines
 ******************************************************************************/
// Spi hardware state variables held in struct
#if 1
MockSpiState_t mockSpiState;
#endif
/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
static void InitialiseMockSpiBus(void)
{
    mockSpiState.initialised = false;
    mockSpiState.connectionOpen = false;
    memset(mockSpiState.readReg, 0U, SPI_DATA_FIFO_LENGTH * sizeof(uint8_t));
    mockSpiState.readIdx = 0U;
}

// write a uint16_t into the spi fifo out buffer MSB first.
static void SetSpiReadReg(uint16_t data)
{
    const uint8_t LSB = data & 0xFF;
    const uint8_t MSB = (data & (0xFF << 8u)) >> 8u;
    mockSpiState.readReg[0u] = MSB;
    mockSpiState.readReg[1u] = LSB;
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

static void SetTC77OverTemp(uint16_t *readReg)
{
    *readReg = (MSB_OVERTEMP + 1U) << 8U
        | (*readReg & 0xFF);
}

// put some data into the spi read register
static void UpdateTC77ReadReg(float temperature)
{
    // convert temp into binary
    const int16_t signedData = (int16_t)(temperature / CONVERSION_FACTOR);

    uint16_t rawData = (uint16_t)(signedData << 3U);
    SetTC77RegReady(&rawData);
    // Put the data into the register
    SetSpiReadReg(rawData);
}

static uint16_t GetTC77RawDataExpected(void)
{
    const uint8_t MSB = mockSpiState.readReg[0U]; 
    const uint8_t LSB = mockSpiState.readReg[1U];
    return ((uint16_t)(MSB << 8U) | LSB);
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
}

// Function to open SPI connection on required CS pin
void OpenSpiConnection(const SpiSettings_t *settings)
{
    // mock function behaviour
    mock().actualCall("OpenSpiConnection")
        .withParameter("settings", (void *)settings);
    mockSpiState.connectionOpen = true;
}

// Transmits and receives a byte - only returns data for now.
uint8_t SpiTransferByte(const uint8_t byteToSpiBus)
{
    // mock function behaviour
    mock().actualCall("SpiTransferByte")
        .withParameter("byteToSpiBus", byteToSpiBus);

    // check readIdx points at valid data
    uint8_t retVal = 0u;
    if (mockSpiState.readIdx < SPI_DATA_FIFO_LENGTH)
    {
        retVal = mockSpiState.readReg[mockSpiState.readIdx];
        mockSpiState.readIdx++;
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
 Test for calling update function on TC77 device after conversion time. We expect
 data to be available and for it to be returned over spi bus. 
 */
TEST(TC77TestGroup, ReadingTC77AfterConversionExpectData)
{
    const uint8_t pinNum = 1U;
    const float mockTemperature = 25.2F;
    UpdateTC77ReadReg(mockTemperature);
    const uint16_t rawDataExpected = GetTC77RawDataExpected();
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;
    
    // Mock calls to low level spi function for init
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    mock().expectOneCall("millis");
    
    TC77_Update();

    const uint16_t rawDataActual = TC77_GetRawData();
    CHECK_EQUAL(rawDataExpected, rawDataActual);
    DOUBLES_EQUAL(mockTemperature, TC77_ConvertToTemp(rawDataActual), 0.1);
    mock().checkExpectations();
}