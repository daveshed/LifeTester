// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "MX7705.h"
#include "MX7705Private.h" // defines and mx7705SpiSettings

// support
#include "Arduino.h"       // arduino function prototypes - implemented MockAduino.c
#include "Config.h"
#include "MockArduino.h"   // mockDigitalPins, mockMillis, private variables
#include "MockSpiCommon.h" // Mock spi interface
#include "SpiCommon.h"     // spi function prototypes - mocks implemented here.
#include <string.h>

#define MAX_CHANNELS             (CH_SELECT_MASK + 1U)
#define MAX_REG_SIZE             (4U)  // max reg width in bytes
#define BUFFER_SIZE              (16U) // mock spi buffer size in bytes 

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
 data here is the wrong type. adcCommsReg is for this purpose.
 */
static CommsRegister_t adcCommsReg;
static IoRegister_t    mx7705Adc[NumberOfEntries];

/* Mock spi buffers used in transferring data
TODO: move to mockSpiState defined in MockSpiCommon.cpp */
static uint8_t inputBuffer[BUFFER_SIZE];
static uint8_t outputBuffer[BUFFER_SIZE];
static uint8_t counter;

// Used to mock read error for verification failure in init for example.
bool mockReadError = false;
// Timing variables used to allow mock polling operations / busy bit control
static const uint32_t conversionTimeDefault = 200U; //ms
static const uint32_t roundtripTimeDefault = 30U; //ms
static uint32_t conversionTime;
static uint32_t roundtripTime;

static uint16_t adcInput; // analog voltage converted to digital
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

// Mocks needed for reading single byte data from mock spi reg
static uint8_t MockForMX7705Read(void)
{
    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
}

// Mocks needed for reading data from mock spi reg
static uint16_t MockForMX7705Read16Bit(void)
{
    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &mx7705SpiSettings);
}

// Mocks for calling MX7705_Init
static void MockForMX7705InitCh0(void)
{
    MockForMX7705Write(MX7705_REQUEST_CLOCK_WRITE_CH0);
    MockForMX7705Write(MX7705_WRITE_CLOCK_SETTINGS);
    MockForMX7705Write(MX7705_REQUEST_SETUP_WRITE_CH0);
    MockForMX7705Write(MX7705_WRITE_SETUP_INIT);
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH0);
    MockForMX7705Read();    
}

// Mocks for calling MX7705_Init
static void MockForMX7705InitCh1(void)
{
    MockForMX7705Write(MX7705_REQUEST_CLOCK_WRITE_CH1);
    MockForMX7705Write(MX7705_WRITE_CLOCK_SETTINGS);
    MockForMX7705Write(MX7705_REQUEST_SETUP_WRITE_CH1);
    MockForMX7705Write(MX7705_WRITE_SETUP_INIT);
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH1);
    MockForMX7705Read();    
}

// Initialises the adc registers with default data from the datasheet.
static void InitAdcRegsData(void)
{
    adcCommsReg = 0U;
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

// Gets read or write op request from the comms reg data
static bool IsReadOp(CommsRegister_t reg)
{
    return bitRead(adcCommsReg, RW_SELECT_OFFSET);   
}

// Get the channel index from the comms reg data 
static uint8_t GetChannel(CommsRegister_t reg)
{
    return adcCommsReg & CH_SELECT_MASK;
}

// Checks the contets of the spi settings data agrees with private data
static void CheckSpiSettings(const SpiSettings_t *settings)
{
    CHECK_EQUAL(CS_DELAY, settings->chipSelectDelay);
    CHECK_EQUAL(SPI_CLOCK_SPEED, settings->clockSpeed);
    CHECK_EQUAL(SPI_BIT_ORDER, settings->bitOrder);
    CHECK_EQUAL(SPI_DATA_MODE, settings->dataMode);
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

// sets DRDY bit high when busy
static void SetCommsRegBusy(CommsRegister_t *reg)
{
    bitSet(*reg, DRDY_BIT);
}

// sets DRDY bit low when not busy
static void ClearCommsRegBusy(CommsRegister_t *reg)
{
    bitClear(*reg, DRDY_BIT);
}

static bool IsCommsRegBusy(CommsRegister_t *reg)
{
    return !bitRead(*reg, DRDY_BIT);
}

// Converts a uint16_t into uint8_t's and loads into data register of adc
static void UpdateDataRegAfterConversion(uint16_t dataToLoad, uint8_t channel)
{
    const uint8_t msb = (dataToLoad >> 8U) & 0xFF;
    const uint8_t lsb = dataToLoad & 0xFF;
    mx7705Adc[DataReg].data[channel][0U] = msb;
    mx7705Adc[DataReg].data[channel][1U] = lsb;
}

// write the data from the input buffer into the io registers
static void TeardownMockSpiConnection(const SpiSettings_t *settings)
{
    CheckSpiSettings(settings);

    const RegisterSelection_t regRequest = GetRequestedRegister(adcCommsReg);

    if (IsReadOp(adcCommsReg))
    {
        // Command done so reset the register.
        adcCommsReg = 0U;
    }
    else //writeop, then data is copied into the relevant reg from inputBuffer
    {
        if (regRequest == CommsReg)
        {
            /*write requested into comms reg - copy from bytes captured in the
            mock spi input buffer*/
            adcCommsReg = inputBuffer[0U];

            // No need to reset the register as we've just written in a command.
        }
        else 
        {
            /*write requested into other reg - copy from bytes captured in the
            mock spi input buffer*/
            memcpy(mx7705Adc[regRequest].data[GetChannel(adcCommsReg)],
                inputBuffer,
                mx7705Adc[regRequest].nBytes);
            
            // Command done so reset the register.
            adcCommsReg = 0U;
        }
    }
}

// Loads data into the spi output buffer if a read is requested in the comms reg
static void SetupMockSpiConnection(const SpiSettings_t *settings)
{
    CheckSpiSettings(settings);

    ResetMockSpiBuffers();

    const RegisterSelection_t regRequest = GetRequestedRegister(adcCommsReg);
    printf("regRequest = %u channel = %u\n",
        (unsigned int)regRequest, GetChannel(adcCommsReg));

    /* 
     If it's a readOp, then load data into the required spi output buffer. 
     
     Mocking of data ready (DRDY) bit 7 of comms reagister is implemented here. 
     Ready bit set to 1 when conversion has finished. And data is ready to be 
     read from the data reg. 

     Read error is mocked by preventing data being copied into spi output.
     */

    if (IsReadOp(adcCommsReg) && !mockReadError)
    {
        switch (regRequest)
        {
            case CommsReg:
                // Timer incremented each time spi is called for polling purposes
                mockMillis += roundtripTime;
                
                printf("mockMillis %lu\n", mockMillis);

                // conversion time is time taken to measure and convert voltage to data
                if (mockMillis > conversionTime)
                {
                    // data has been converted so we can clear busy bit
                    ClearCommsRegBusy(&adcCommsReg);
                    // and put the ADC data into the data register
                    UpdateDataRegAfterConversion(adcInput, GetChannel(adcCommsReg));
                }
                else
                {
                    // data not converted yet
                    SetCommsRegBusy(&adcCommsReg);
                }
                /* Data copied into the mock spi output buffer to be transferred */
                outputBuffer[0U] = adcCommsReg;  // only 1 byte
                break;
            case DataReg:
                if (IsCommsRegBusy(&adcCommsReg))
                {
                    /* Data only copied into data register when conversion has 
                     happened. */
                    memcpy(outputBuffer,
                        mx7705Adc[DataReg].data[GetChannel(adcCommsReg)],
                        mx7705Adc[DataReg].nBytes);
                }
            default:
                /* Data copied into the mock spi output buffer to be transffered
                from the requested register. */
                memcpy(outputBuffer,
                    mx7705Adc[regRequest].data[GetChannel(adcCommsReg)],
                    mx7705Adc[regRequest].nBytes);
                break;
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
        InitAdcRegsData();
        mockMillis = 0U;
        
        SpiTransferByte_Callback = &TransferMockSpiData;
        OpenSpiConnection_Callback = &SetupMockSpiConnection;
        CloseSpiConnection_Callback = &TeardownMockSpiConnection;
        
        conversionTime = conversionTimeDefault;
        roundtripTime = roundtripTimeDefault;
        mockReadError = false;

        adcInput = 0U;
    }

    void teardown(void)
    {
        mock().clear();
    }
};
#if 0
// Test for initialising MX7705 device on channel 0.
TEST(MX7705TestGroup, InitialiseMX7705ChannelZero)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    
    CHECK_EQUAL(0xFF, mx7705SpiSettings.chipSelectPin);
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh0();
    
    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    MX7705_Init(pinNum, channel);

    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    const uint8_t clockRegExp = MX7705_WRITE_CLOCK_SETTINGS;
    const uint8_t clockRegAct = mx7705Adc[ClockReg].data[channel][0U];
    CHECK_EQUAL(clockRegExp, clockRegAct);

    const uint8_t setupRegExp = MX7705_WRITE_SETUP_INIT;
    const uint8_t setupRegAct = mx7705Adc[SetupReg].data[channel][0U];
    CHECK_EQUAL(setupRegExp, setupRegAct);

    CHECK_EQUAL(false, MX7705_GetError()); 
    CHECK_EQUAL(pinNum, mx7705SpiSettings.chipSelectPin = pinNum);

    // check function calls
    mock().checkExpectations();
}

// Test for initialising MX7705 device on channel 1.
TEST(MX7705TestGroup, InitialiseMX7705ChannelOne)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    CHECK_EQUAL(0xFF, mx7705SpiSettings.chipSelectPin);
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    MX7705_Init(pinNum, channel);

    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    const uint8_t clockRegExp = MX7705_WRITE_CLOCK_SETTINGS;
    const uint8_t clockRegAct = mx7705Adc[ClockReg].data[channel][0U];
    CHECK_EQUAL(clockRegExp, clockRegAct);

    const uint8_t setupRegExp = MX7705_WRITE_SETUP_INIT;
    const uint8_t setupRegAct = mx7705Adc[SetupReg].data[channel][0U];
    CHECK_EQUAL(setupRegExp, setupRegAct);

    CHECK_EQUAL(false, MX7705_GetError()); 
    CHECK_EQUAL(pinNum, mx7705SpiSettings.chipSelectPin);

    // check function calls
    mock().checkExpectations();
}

/*
 Test for initialising MX7705 device on channel 1. Error condition is raised
 because there is a failure to verify data stored in setup reg.
 */
TEST(MX7705TestGroup, InitialiseMX7705ChannelOneVerifyFail)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    CHECK_EQUAL(0xFF, mx7705SpiSettings.chipSelectPin);
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    // allows us to mock a read fail which triggers verification fail
    mockReadError = true;
    // call function under test
    MX7705_Init(pinNum, channel);

    CHECK_EQUAL(pinNum, mx7705SpiSettings.chipSelectPin);
    
    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);

    CHECK_EQUAL(true, MX7705_GetError()); 

    // check function calls
    mock().checkExpectations();
}
#endif
#if 1
/*
 Read data on channel 1
 */
TEST(MX7705TestGroup, ReadDataCh1Ok)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);
    printf("clock reg %u\n", mx7705Adc[SetupReg].data[channel][0U]);

    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    printf("comms reg %u\n", adcCommsReg);
    printf("clock reg %u\n", mx7705Adc[ClockReg].data[channel][0U]);
    printf("clock reg %u\n", mx7705Adc[SetupReg].data[channel][0U]);

    // Mocks for calling read data function
    mock().expectOneCall("millis");    
    // Mocks for polling comms reg
    for (int i = 0; i <= (conversionTime / roundtripTime); i++)
    {
        mock().expectOneCall("millis");    
        MockForMX7705Write(MX7705_REQUEST_COMMS_READ_CH1);
        MockForMX7705Read();        
    }
    // Mocks for reading data
    MockForMX7705Write(MX7705_REQUEST_DATA_READ_CH1);
    MockForMX7705Read16Bit();        
    
    // Set an input to the adc and call function under test
    adcInput = 25667U;
    CHECK_EQUAL(adcInput, MX7705_ReadData(channel));
    // expect error condition due to measurement timeout
    CHECK_EQUAL(false, MX7705_GetError()); 

    // check function calls
    mock().checkExpectations();
}
#endif