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

/*
 Type holds all data contained in MX7705 i/o registers. Up to four channels are
 available however hardware implements two channels: one to read the current for
 each device under test.
 chA = Ain1+/-
 chB = Ain2+/-
 Note that a single register can be 8, 16 or 24 bits wide and must have space for
 all possible channels.
*/
typedef struct IoRegister_s {
    uint8_t data[MAX_CHANNELS][MAX_REG_SIZE];
    uint8_t nBytes; //TODO make this const
} IoRegister_t;

// There is only one comms register which is one byte wide.
typedef uint8_t CommsRegister_t;

/*
 Create Adc variables to hold all registers relating to the MX7705. Note that 
 the comms register is a separate type. Do not access mx7705Adc[CommsReg]. The
 data here is the wrong type. mx7705AdcCommsReg is for this purpose.
 */
static CommsRegister_t mx7705AdcCommsReg;
static IoRegister_t    mx7705Adc[NumberOfEntries];

/* Mock spi buffers used in transferring data
TODO: move to mockSpiState defined in MockSpiCommon.cpp */
static uint8_t inputBuffer[BUFFER_SIZE];
static uint8_t outputBuffer[BUFFER_SIZE];
static uint8_t counter;

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

// Mocks for calling MX7705_Init
static void MockForMX7705Init(void)
{
    MockForMX7705Write(MX7705_REQUEST_CLOCK_WRITE);
    MockForMX7705Write(MX7705_WRITE_CLOCK_SETTINGS);
    MockForMX7705Write(MX7705_REQUEST_SETUP_WRITE_CH0);
    MockForMX7705Write(MX7705_WRITE_SETUP_INIT);
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH0);
    MockForMX7705Read();    
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

    // TODO: four channels initialised but what if the number changes
    memcpy(mx7705Adc, initData, (sizeof(IoRegister_t) * NumberOfEntries));
}

// reset the buffers and counter
static void ResetMockSpiBuffers(void)
{
    memset(inputBuffer, 0U, BUFFER_SIZE);
    memset(outputBuffer, 0U, BUFFER_SIZE);
    counter = 0U;    
}

// Gets the requested register from comms reg data
static RegisterSelection_t GetRequestedRegister(CommsRegister_t reg)
{
    return (RegisterSelection_t)((reg >> REG_SELECT_OFFSET) & REG_SELECT_MASK);
    
}

static bool IsReadOp(CommsRegister_t reg)
{
    return bitRead(mx7705AdcCommsReg, RW_SELECT_OFFSET);   
}

static uint8_t GetChannel(CommsRegister_t reg)
{
    return mx7705AdcCommsReg & CH_SELECT_MASK;
}

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
    const RegisterSelection_t regRequest = GetRequestedRegister(mx7705AdcCommsReg);

    if (IsReadOp(mx7705AdcCommsReg))
    {
        // Command done so reset the register.
        mx7705AdcCommsReg = 0U;
    }
    else
    {
        if (regRequest == CommsReg)
        {
            /*write requested into comms reg - copy from bytes captured in the
            mock spi input buffer*/
            mx7705AdcCommsReg = inputBuffer[0U];

            // No need to reset the register as we've just written in a command.
        }
        else //writeop, then data is copied into the relevant reg from inputBuffer
        {
            /*write requested into other reg - copy from bytes captured in the
            mock spi input buffer*/
            memcpy(mx7705Adc[regRequest].data[GetChannel(mx7705AdcCommsReg)],
                inputBuffer,
                mx7705Adc[regRequest].nBytes);
            
            // Command done so reset the register.
            mx7705AdcCommsReg = 0U;
        }
    }
}

// Loads data into the spi output buffer if a read is requested in the comms reg
static void SetupMockSpiConnection(const SpiSettings_t *settings)
{
    ResetMockSpiBuffers();

    const RegisterSelection_t regRequest = GetRequestedRegister(mx7705AdcCommsReg);
    printf("regRequest = %u\n", (unsigned int)regRequest);

    // if it's a readOp, then load data into the output buffer
    if (IsReadOp(mx7705AdcCommsReg))
    {
        if (regRequest == CommsReg)
        {
            /* Data copied into the mock spi output buffer to be transffered */
            outputBuffer[0U] = mx7705AdcCommsReg;  // only 1 byte
        }
        else
        {
            /* Data copied into the mock spi output buffer to be transffered */
            memcpy(outputBuffer,
                mx7705Adc[regRequest].data[GetChannel(mx7705AdcCommsReg)],
                mx7705Adc[regRequest].nBytes);
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
    
    MockForMX7705Init();
    
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

    const uint8_t clockRegExp = MX7705_WRITE_CLOCK_SETTINGS;
    const uint8_t clockRegAct = mx7705Adc[ClockReg].data[channel][0U];
    CHECK_EQUAL(clockRegExp, clockRegAct);

    const uint8_t setupRegExp = MX7705_WRITE_SETUP_INIT;
    const uint8_t setupRegAct = mx7705Adc[SetupReg].data[channel][0U];
    CHECK_EQUAL(setupRegExp, setupRegAct);

    CHECK_EQUAL(false, MX7705_GetError()); 

    // check function calls
    mock().checkExpectations();
}