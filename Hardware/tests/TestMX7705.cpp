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

// default register contents
#define COMMS_REG_DEFAULT        (0U)
#define SETUP_REG_DEFAULT        (1U)
#define CLOCK_REG_DEFAULT        (5U)
#define DATA_REG_DEFAULT         (0U)
#define COMMS_REG_BYTES          (1U)
#define SETUP_REG_BYTES          (1U)
#define CLOCK_REG_BYTES          (1U)
#define DATA_REG_BYTES           (2U)

// Useful initialiser - note that this is only valid when there are 4 channels
#if (MAX_CHANNELS == 4)
    #define IO_REG_INIT(data, n)     {{{data}, {data}, {data}, {data}}, n}
#else
    #error "Register initialised with incompatible number of channels."
#endif

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
    uint8_t       data[MAX_CHANNELS][MAX_REG_SIZE];
    const uint8_t nBytes;
} IoRegister_t;

// There is only one comms register which is one byte wide.
typedef uint8_t CommsRegister_t;

/*
 Create Adc variables to hold all registers relating to the MX7705. Note that 
 the comms register is a separate type. Do not access mx7705Adc[CommsReg]. The
 data here is the wrong type. adcCommsReg is for this purpose. Because nBytes
 is a const, we need to initialise with data here.
 */
static CommsRegister_t adcCommsReg;
static IoRegister_t    mx7705Adc[NumberOfEntries] = 
        { {0U},       // CommsReg - placeholder don't use this
          {{0U}, 1U}, // SetupReg
          {{0U}, 1U}, // ClockReg
          {{0U}, 2U}, // DataReg
        };  // other elements will be initialised to 0U.

/* Mock spi buffers used in transferring data
TODO: move to mockSpiState defined in MockSpiCommon.cpp */
static uint8_t inputBuffer[BUFFER_SIZE];
static uint8_t outputBuffer[BUFFER_SIZE];
static uint8_t counter;

// Used to mock read error for verification failure in init for example.
static bool mockReadError = false;
// Mocks timeout error - comms reg never sets ready status.
static bool triggerTimeout;

// Timing variables used to allow mock polling operations / busy bit control
static const uint32_t conversionTimeDefault = 200U; //ms
static const uint32_t roundtripTimeDefault = 30U; //ms
static uint32_t conversionTime;
static uint32_t roundtripTime;
static uint32_t elapsedTime; // timer to keep track of data conversion

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

// Mocks for calling read data function
static void MockForMX7705DataReadCh0(void)
{   
    MockForMX7705Write(MX7705_REQUEST_DATA_READ_CH0);
    MockForMX7705Read16Bit();        
}

// Mocks for calling read data function
static void MockForMX7705DataReadCh1(void)
{   
    MockForMX7705Write(MX7705_REQUEST_DATA_READ_CH1);
    MockForMX7705Read16Bit();        
}

// Mocks for polling comms reg ch0
static void MockForMX7705PollingCh0(void)
{
    const int nPolls = triggerTimeout ?
        TIMEOUT_MS / roundtripTime:
        conversionTime / roundtripTime;

    printf("\n\nnPolls = %u\n\n", nPolls);

    mock().expectOneCall("millis");    
    /* 
     Mocks for polling comms reg - only poll within timeout or conversion time.
     Note that another poll is expected since we have a do while loop - poll
     happens before time is checked.
    */
    for (int i = 0; (i <= nPolls + 1); i++)
    {
        mock().expectOneCall("millis");    
        MockForMX7705Write(MX7705_REQUEST_COMMS_READ_CH0);
        MockForMX7705Read();        
    }
}    

// Mocks for polling comms reg ch1
static void MockForMX7705PollingCh1(void)
{
    const int nPolls = triggerTimeout ?
        TIMEOUT_MS / roundtripTime:
        conversionTime / roundtripTime;

    printf("\n\nnPolls = %u\n\n", nPolls);

    mock().expectOneCall("millis");    
    /* 
     Mocks for polling comms reg - only poll within timeout or conversion time.
     Note that another poll is expected since we have a do while loop - poll
     happens before time is checked.
    */
    for (int i = 0; (i <= nPolls + 1); i++)
    {
        mock().expectOneCall("millis");    
        MockForMX7705Write(MX7705_REQUEST_COMMS_READ_CH1);
        MockForMX7705Read();        
    }
}    

static void SetAdcRegData(IoRegister_t *adc, RegisterSelection_t reg, uint8_t channel, uint32_t val)
{
    // MSB first
    const uint8_t regData[MAX_REG_SIZE] =
        {
            (val >> 24U) & 0xFF,
            (val >> 16U) & 0xFF,
            (val >> 8U) & 0xFF,
            val & 0xFF,
        };
    memcpy(adc[reg].data[channel], regData, MAX_REG_SIZE * sizeof(uint8_t));
}


// Initialises the adc registers with default data from the datasheet.
static void InitAdcRegsData(void)
{
    adcCommsReg = COMMS_REG_DEFAULT;

    // Taken from MX7705 data sheet - see table 7 register selection
    const IoRegister_t initData[NumberOfEntries] = 
        { {0U},                // CommsReg - placeholder don't use this
          IO_REG_INIT(1U, 1U), // SetupReg
          IO_REG_INIT(5U, 1U),  // ClockReg
          IO_REG_INIT(0U, 1U), // DataReg
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
    return (RegisterSelection_t) bitExtract(reg, REG_SELECT_MASK, REG_SELECT_OFFSET);
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

// sets DRDY -> 1 meaning busy
static void SetCommsRegBusy(CommsRegister_t *reg)
{
    printf("Comms busy\n");
    bitSet(*reg, DRDY_BIT);
}

// sets DRDY -> 0 meaning ready
static void ClearCommsRegBusy(CommsRegister_t *reg)
{
    printf("Comms ready\n");
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
                
                printf("mockMillis %lu elapsedTime %u\n", mockMillis, elapsedTime);

                // conversion time is time taken to measure and convert voltage to data
                if ((elapsedTime > conversionTime) && !triggerTimeout)
                {
                    // data has been converted so we can clear busy bit
                    ClearCommsRegBusy(&adcCommsReg);
                    // and put the ADC data into the data register
                    UpdateDataRegAfterConversion(adcInput, GetChannel(adcCommsReg));
                    // reset timer to prepare for the next measurement
                    elapsedTime = 0U;
                }
                else
                {
                    // Timer incremented each time spi is called for polling purposes
                    mockMillis += roundtripTime;
                    elapsedTime += roundtripTime;
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
        elapsedTime = 0U;
        
        SpiTransferByte_Callback = &TransferMockSpiData;
        OpenSpiConnection_Callback = &SetupMockSpiConnection;
        CloseSpiConnection_Callback = &TeardownMockSpiConnection;
        
        conversionTime = conversionTimeDefault;
        roundtripTime = roundtripTimeDefault;
        mockReadError = false;
        triggerTimeout = false;

        adcInput = 0U;
    }

    void teardown(void)
    {
        mock().clear();
    }
};

/*
 Test for initialising MX7705 device on channel 0. 
 */
TEST(MX7705TestGroup, InitialiseAdcChannelZero)
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
TEST(MX7705TestGroup, InitialiseAdcChannelOne)
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
TEST(MX7705TestGroup, InitialiseAdcChannelOneVerifyFail)
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

/*
 Read data on channel 0. No errors expected. Captures device polling of DRDY bit
 by incrementing timer in setup spi connection.
 */
TEST(MX7705TestGroup, ReadDataChZeroOkPollingDrdyBit)
{
    const uint8_t pinNum = 2U;
    const uint8_t channel = 0U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh0();
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // Mocks needed to call function under test
    MockForMX7705PollingCh0();
    MockForMX7705DataReadCh0();

    // Set an input to the adc and call function under test
    adcInput = 25667U;
    CHECK_EQUAL(adcInput, MX7705_ReadData(channel));
    // expect error condition due to measurement timeout
    CHECK_EQUAL(false, MX7705_GetError()); 

    // check function calls
    mock().checkExpectations();
}

/*
 Read data on channel 1. No errors expected. Captures device polling of DRDY bit
 by incrementing timer in setup spi connection.
 */
TEST(MX7705TestGroup, ReadDataChOneOkPollingDrdyBit)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // Check that there is no data in the Adc data reg to begin with
    MockForMX7705PollingCh1();
    MockForMX7705DataReadCh1();
    CHECK_EQUAL(0U, adcInput);
    CHECK_EQUAL(adcInput, MX7705_ReadData(channel));
    // expect error condition due to measurement timeout
    CHECK_EQUAL(false, MX7705_GetError());     

    // Set an input to the adc and call function under test
    MockForMX7705PollingCh1();
    MockForMX7705DataReadCh1();
    adcInput = 25667U;
    CHECK_EQUAL(adcInput, MX7705_ReadData(channel));
    // expect error condition due to measurement timeout
    CHECK_EQUAL(false, MX7705_GetError()); 

    // check function calls
    mock().checkExpectations();
}

/*
 Read data on channel 0. Trigger timeout error by using flag. Stops DRDY bit 
 changing to data ready. Expect raise error condition.
 */
TEST(MX7705TestGroup, ReadDataChZeroPollingTimeoutErrorCondition)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh0();
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // mock input
    adcInput = 2456U;
    
    // Set timout error flag
    triggerTimeout = true;
    
    // Mock/expects function calls
    MockForMX7705PollingCh0();
    
    // Check data register is not changed by reading data due to timeout error
    const uint16_t dataToExpect = 
        (mx7705Adc[DataReg].data[channel][0U] << 8U)
        | mx7705Adc[DataReg].data[channel][0U];

    // Call funtion under test and record as actual data
    const uint16_t dataActual = MX7705_ReadData(channel);
    
    // compare expected and actual data
    CHECK_EQUAL(dataToExpect, dataActual);
    // expect error condition due to measurement timeout
    CHECK_EQUAL(true, MX7705_GetError());     

    // check function calls
    mock().checkExpectations();
}

/*
 Read data on channel 0. Trigger timeout error by using flag. Stops DRDY bit 
 changing to data ready. Expect raise error condition.
 */
TEST(MX7705TestGroup, ReadDataChOnePollingTimeoutErrorCondition)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // mock input
    adcInput = 2456U;
    
    // Set timout error flag
    triggerTimeout = true;
    
    // Mock/expects function calls
    MockForMX7705PollingCh1();
    
    // Check data register is not changed by reading data due to timeout error
    const uint16_t dataToExpect = 
        (mx7705Adc[DataReg].data[channel][0U] << 8U)
        | mx7705Adc[DataReg].data[channel][0U];

    // Call funtion under test and record as actual data
    const uint16_t dataActual = MX7705_ReadData(channel);
    
    // compare expected and actual data
    CHECK_EQUAL(dataToExpect, dataActual);
    // expect error condition due to measurement timeout
    CHECK_EQUAL(true, MX7705_GetError());     

    // check function calls
    mock().checkExpectations();
}

// Test for getting and setting gain on channel 1.
TEST(MX7705TestGroup, SetGetGainChannelOne)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    MX7705_Init(pinNum, channel);

    // Mocks for calling get gain
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH1);
    MockForMX7705Read();

    // Read the initial gain. Should match the settings stored in the setup reg
    const uint8_t setupRegInitial = mx7705Adc[SetupReg].data[channel][0U];
    uint8_t gainActual = MX7705_GetGain(channel);
    uint8_t gainExpected = bitExtract(setupRegInitial, PGA_MASK, PGA_OFFSET);
    CHECK_EQUAL(gainExpected, gainActual);

    // Work out the expected setup register after changing gain.
    uint8_t setupRegExpected = setupRegInitial;
    // increment gain to be written back into the setup reg.
    gainExpected++;
    // write in the new gain
    bitInsert(setupRegExpected, gainExpected, PGA_MASK, PGA_OFFSET);
    // mocks for calling set gain
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH1);
    MockForMX7705Read();
    MockForMX7705Write(MX7705_REQUEST_SETUP_WRITE_CH1);
    MockForMX7705Write(setupRegExpected);

    // Call to set gain
    MX7705_SetGain(gainExpected, channel);
    
    // Now call get gain again to check that the gain has actually been written.

    // Mocks for calling get gain
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH1);
    MockForMX7705Read();

    // check the gain returned matches gain requested
    gainActual = MX7705_GetGain(channel);
    CHECK_EQUAL(gainExpected, gainActual);

    // check that the setup register matches expectations
    const uint8_t setupRegActual = mx7705Adc[SetupReg].data[channel][0U];
    CHECK_EQUAL(setupRegExpected, setupRegActual);

    // check function calls
    mock().checkExpectations();
}


/*
 Test for getting and setting gain on channel 1 outside allowed range. Maximum
 allowed gain setting is 7. Here we set to 8. Gain should not change. Command
 is ignored.
*/
TEST(MX7705TestGroup, SetGetGainOutsideRangeCommandIgnoredChannelOne)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 1U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705InitCh1();
    
    MX7705_Init(pinNum, channel);

    // Get the gain and setup reg data to begin with...
    // Mocks for calling get gain
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH1);
    MockForMX7705Read();
    const uint8_t setupRegInitial = mx7705Adc[SetupReg].data[channel][0U];
    const uint8_t gainInitial = MX7705_GetGain(channel);

    // now request gain outside range
    const uint8_t gainRequested = PGA_MASK + 1U;
    MX7705_SetGain(gainRequested, channel);
    
    // Get the gain and setup reg data again to compare against...
    // Mocks for calling get gain
    MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH1);
    MockForMX7705Read();
    // check the gain returned matches gain requested
    const uint8_t setupRegFinal = mx7705Adc[SetupReg].data[channel][0U];
    const uint8_t gainFinal = MX7705_GetGain(channel);

    CHECK_EQUAL(gainFinal, gainInitial);
    CHECK_EQUAL(setupRegFinal, setupRegInitial);

    // check function calls
    mock().checkExpectations();
}