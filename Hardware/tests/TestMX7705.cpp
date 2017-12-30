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
#include <string.h>

#define REG_SELECT_OFFSET   (4U)
#define REG_SELECT_MASK     (7U)

#define CH0_OFFSET          (0U)
#define CH1_OFFSET          (1U)

#define RW_SELECT_OFFSET    (3U)

#define CH_SELECT_MASK      (3U)
#define MAX_CHANNELS        (CH_SELECT_MASK + 1U)
#define MAX_REG_SIZE        (4U)  // max reg width in bytes
#define BUFFER_SIZE         (16U)

/*
 enum containing all available registers that can be selected from RS0-2 bits
 of the comms register.
*/
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

/* A single register can be 8, 16 or 24 bits wide and must have space for all 
possible channels */
typedef struct IoRegister_s {
    uint8_t data[MAX_CHANNELS][MAX_REG_SIZE];
    uint8_t nBytes; //TODO make this const
} IoRegister_t;

#if 0
/*There is only one comms register which is one byte wide. It's padded to the 
 same width as other IoRegisters which means that all registers can be stored in
 an array - see below.*/
typedef struct CommsRegister_s {
    uint8_t padding[sizeof(IoRegister_t) - sizeof(uint8_t)];  // TODO: make const
    uint8_t data;
} CommsRegister_t;
#endif
typedef uint8_t CommsRegister_t;

#if 0
/*
 Up to four channels can be accessed from bits 0 and 1 in the comms reg. This
 means that there needs to be space for this data.
*/
typedef struct MultiChannelReg_s {
    IoRegister_t chA;
    IoRegister_t chB;
} MultiChannelReg_t;
/*
 Union of a single and multi channel io register so that each register can be
 addressed.  Type holds all data contained in MX7705 i/o registers. Up to four
 channels are available however hardware implements two channels: one to
 read the current for each device under test.
 chA = Ain1+/-
 chB = Ain2+/-
*/
typedef union AdcIoRegister_u {
    IoRegister_t singleCh;
    IoRegister_t multiCh[MAX_CHANNELS];
} AdcIoRegister_t;
#endif

// Create Adc variable to hold all registers relating to the MX7705
CommsRegister_t mx7705AdcCommsReg;
IoRegister_t    mx7705Adc[NumberOfEntries];

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

// Initialises the adc registers with default data from the datasheet.
static void InitAdcRegsData(void)
{
    mx7705AdcCommsReg = 0U;
    // Taken from MX7705 data sheet - see table 7 register selection
    const IoRegister_t initData[NumberOfEntries] = 
        { {0U},                           // CommsReg - placeholder don't use this
          {{{1U}, {1U}, {1U}, {1U}}, 1U}, // SetupReg
          {{{5U}, {5U}, {5U}, {5U}}, 1U}, // ClockReg
          {{{0U}, {0U}, {0U}, {0U}}, 2U}, // DataReg
        };  // other elements will be initialised to 0U.

    // TODO: only first two channels are initialised
    memcpy(mx7705Adc, initData, (sizeof(IoRegister_t) * NumberOfEntries));
}
    static uint8_t inputBuffer[BUFFER_SIZE];
    static uint8_t outputBuffer[BUFFER_SIZE];
    static uint8_t counter;

/*
 Manages commands transferred to the spi bus and data is returned to the output.
 A pointer to this function is passed to the source when SpiTransferByte is called.
 */
static uint8_t TransferMockSpiData(uint8_t byteReceived)
{
    inputBuffer[counter] = byteReceived;
    const uint8_t retVal = outputBuffer[counter];
    printf("byteReceived %u byteToTransmit %u\n", byteReceived, retVal);
    counter++;
    return retVal;
}

// write the data from the input buffer into the io registers
static void TeardownMockSpiConnection(const SpiSettings_t *settings)
{
    const RegisterSelection_t requestedRegister = 
        (RegisterSelection_t)((mx7705AdcCommsReg >> REG_SELECT_OFFSET) & REG_SELECT_MASK);

    // if it's a readOp, then load data into the output buffer
    const bool writeOp = !bitRead(mx7705AdcCommsReg, RW_SELECT_OFFSET);

    const uint8_t ch = mx7705AdcCommsReg & CH_SELECT_MASK;
    if (writeOp)
    {
        if (requestedRegister == CommsReg)
        {
            /*write requested into comms reg - copy from bytes captured in the
            mock spi input buffer*/
            mx7705AdcCommsReg = inputBuffer[0U];
        }
        else
        {
            /*write requested into other reg - copy from bytes captured in the
            mock spi input buffer*/
            memcpy(mx7705Adc[requestedRegister].data[ch],
                inputBuffer,
                mx7705Adc[requestedRegister].nBytes);
            
            mx7705AdcCommsReg = 0U;
        }
    }
    else
    {
        mx7705AdcCommsReg = 0U;
    }
}

// 
static void SetupMockSpiConnection(const SpiSettings_t *settings)
{
    // reset the buffers and counter
    memset(inputBuffer, 0U, BUFFER_SIZE);
    memset(outputBuffer, 0U, BUFFER_SIZE);
    counter = 0U;

    const RegisterSelection_t requestedRegister = 
        (RegisterSelection_t)((mx7705AdcCommsReg >> REG_SELECT_OFFSET) & REG_SELECT_MASK);
    printf("requestedRegister = %u\n", (unsigned int)requestedRegister);
    
    const bool readOp = bitRead(mx7705AdcCommsReg, RW_SELECT_OFFSET);   
    const uint8_t ch = mx7705AdcCommsReg & CH_SELECT_MASK;

    // if it's a readOp, then load data into the output buffer
    if (readOp)
    {
        if (requestedRegister == CommsReg)
        {
            /* Data copied into the mock spi output buffer to be transffered */
            outputBuffer[0U] = mx7705AdcCommsReg;  // only 1 byte
        }
        else
        {
            /* Data copied into the mock spi output buffer to be transffered */
            memcpy(outputBuffer,
                mx7705Adc[requestedRegister].data[ch],
                mx7705Adc[requestedRegister].nBytes);
        }
    }

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
        SpiTransferByte_Callback = &TransferMockSpiData;
        OpenSpiConnection_Callback = &SetupMockSpiConnection;
        CloseSpiConnection_Callback = &TeardownMockSpiConnection;
        InitAdcRegsData();
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
    
    printf("comms reg %u\n", mx7705AdcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    MX7705_Init(pinNum, channel);
#if 0
    CHECK_EQUAL(CS_DELAY, tc77SpiSettings.chipSelectDelay);
    CHECK_EQUAL(SPI_CLOCK_SPEED, tc77SpiSettings.clockSpeed);
    CHECK_EQUAL(SPI_BIT_ORDER, tc77SpiSettings.bitOrder);
    CHECK_EQUAL(SPI_DATA_MODE, tc77SpiSettings.dataMode);
#endif

    printf("comms reg %u\n", mx7705AdcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

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