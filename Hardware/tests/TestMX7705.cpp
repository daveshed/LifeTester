// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "MX7705.h"
#include "MX7705Private.h" // defines and mx7705SpiSettings

// support
#include "Arduino.h"       // arduino function prototypes - implemented MockAduino.c
#include "MockArduino.h"   // mockDigitalPins, mockMillis, private variables
#include "MockSpiCommon.h" // Mock spi interface
#include "SpiCommon.h"     // spi function prototypes - mocks implemented here.

#define REG_SELECT_OFFSET   (4U)
#define REG_SELECT_MASK     (7U)

#define CH0_OFFSET          (0U)
#define CH1_OFFSET          (1U)

#define RW_SELECT_OFFSET    (3U)

typedef enum RegisterSelection_e {
    CommsReg,
    SetupReg,
    ClockReg,
    DataReg,
    TestReg,
    NoOperation,
    OffsetReg,
    GainReg,
    NumberOfEntries
} RegisterSelection_t;


/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
// Mocks needed for writing data into mock spi reg
static void MockForMX7705Write(uint8_t sendByte)
{
    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", sendByte);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
}

// Mocks needed for reading data from mock spi reg
static uint8_t MockForMX7705Read(void)
{
    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
}

uint8_t commsRegData;
uint8_t clockRegDataCh1;
uint8_t clockRegDataCh2;
uint8_t setupRegDataCh1;
uint8_t setupRegDataCh2;

/*
 Manages commands transferred to the spi bus and data is returned to the output.
 A pointer to this function is passed to the source when SpiTransferByte is called.
 */
static uint8_t UpdateMockSpiRegs(uint8_t byteReceived)
{
    // Check data in comms reg and determine register selection
    RegisterSelection_t requestedRegister = 
        (RegisterSelection_t)((commsRegData >> REG_SELECT_OFFSET) & REG_SELECT_MASK);
    printf("requestedRegister = %u\n", (unsigned int)requestedRegister);
    printf( "byteReceived %u\n", byteReceived);
    // Which channel is selected
    bool chZeroSelected = bitRead(commsRegData, CH0_OFFSET);
    bool chOneSelected = bitRead(commsRegData, CH1_OFFSET);
    // Only modes 0 (AIN1+/-) and 1(AIN2+/-) implemented at present
    CHECK(!chOneSelected);

    // Read or write
    bool writeOp = !bitRead(commsRegData, RW_SELECT_OFFSET);
    
    // static uint8_t byteStored;
    uint8_t byteToTransmit = 0U;

    switch(requestedRegister)
    {
        case CommsReg:
            if (writeOp)
            {
                commsRegData = byteReceived;
            }
            else //read operation
            {
                byteToTransmit = commsRegData;
            }
            break;
        case SetupReg:
            if (writeOp)
            {
                chZeroSelected ?
                    (setupRegDataCh2 = byteReceived):
                    (setupRegDataCh1 = byteReceived);
            }
            else //read operation
            {
                byteToTransmit = chZeroSelected ? setupRegDataCh2 : setupRegDataCh1;
            }
            commsRegData = 0U;  // now reset to default ready for next command
            break;
        case ClockReg:
            if (writeOp)
            {
                chZeroSelected ?
                    (clockRegDataCh2 = byteReceived):
                    (clockRegDataCh1 = byteReceived);
            }
            else //read operation
            {
                byteToTransmit = chZeroSelected ? clockRegDataCh2 : clockRegDataCh1;
            }
            commsRegData = 0U;  // now reset to default
            break;
        default:
            break;
    }
    printf("byteToTransmit = %u\n", byteToTransmit);
    return byteToTransmit;
}

/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group - all tests share common setup/teardown
TEST_GROUP(MX7705TestGroup)
{
    void setup(void)
    {
        InitialiseMockSpiBus(&mx7705SpiSettings);
        mockMillis = 0U;
        SpiTransferByte_Callback = &UpdateMockSpiRegs;
    }

    void teardown(void)
    {
        mock().clear();
    }
};

// Test for initialising MX7705 device.
TEST(MX7705TestGroup, InitialiseMX7705)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705Write(MX7705_REQUEST_CLOCK_WRITE);
    MockForMX7705Write(MX7705_WRITE_CLOCK_SETTINGS);
    MockForMX7705Write(MX7705_REQUEST_SETUP_WRITE_CH0);
    MockForMX7705Write(MX7705_WRITE_SETUP_INIT);
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH0);
    MockForMX7705Read();
    
    printf("comms reg %u\n", commsRegData);
    printf("clock reg %u\n", clockRegDataCh1);

    MX7705_Init(pinNum, channel);
#if 0
    CHECK_EQUAL(CS_DELAY, tc77SpiSettings.chipSelectDelay);
    CHECK_EQUAL(SPI_CLOCK_SPEED, tc77SpiSettings.clockSpeed);
    CHECK_EQUAL(SPI_BIT_ORDER, tc77SpiSettings.bitOrder);
    CHECK_EQUAL(SPI_DATA_MODE, tc77SpiSettings.dataMode);
#endif
    printf("comms reg %u\n", commsRegData);
    printf("clock reg %u\n", clockRegDataCh1);
    // check function calls
    mock().checkExpectations();
}

#if 0
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
    UpdateTC77ReadReg(mockTemperature, true);
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
#endif