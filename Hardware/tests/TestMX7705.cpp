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
    #define IO_REG_INIT(data)     {data, data, data, data}
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
typedef struct AdcIoRegisters_s {
    uint8_t commsReg;
    uint8_t setupReg[MAX_CHANNELS];
    uint8_t clockReg[MAX_CHANNELS];
    uint16_t dataReg[MAX_CHANNELS];
} AdcIoRegisters_t;

static AdcIoRegisters_t mx7705Adc; 

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

static uint16_t adcInput; // analog voltage converted to digital data
/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/

// Initialises the adc registers with default data from the datasheet.
static void InitAdcRegsData(void)
{
    const AdcIoRegisters_t adcInit = {
        COMMS_REG_DEFAULT,
        IO_REG_INIT(SETUP_REG_DEFAULT),
        IO_REG_INIT(CLOCK_REG_DEFAULT),
        IO_REG_INIT(DATA_REG_DEFAULT)
    };

    mx7705Adc = adcInit;
}

// reset the buffers and counter
static void ResetMockSpiBuffers(void)
{
    memset(inputBuffer, 0U, BUFFER_SIZE);
    memset(outputBuffer, 0U, BUFFER_SIZE);
    counter = 0U;    
}

// Gets the requested register from comms reg data
static RegisterSelection_t GetRequestedRegister(uint8_t commsReg)
{
    return (RegisterSelection_t) bitExtract(commsReg, REG_SELECT_MASK, REG_SELECT_OFFSET);
}

// Gets read or write op request from the comms reg data
static bool IsReadOp(uint8_t commsReg)
{
    return bitRead(commsReg, RW_BIT);   
}

// Get the channel index from the comms reg data 
static uint8_t GetChannel(uint8_t commsReg)
{
    return bitExtract(commsReg, CH_SELECT_MASK, CH_SELECT_OFFSET);
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
    counter++;
    return retVal;
}

/*
 Loads data (digital converted voltage) in the adc object from the selected 
 channel (comms register) into the output buffer. 
*/
static void LoadAdcDataIntoByteBuf(uint8_t *buf, AdcIoRegisters_t *adc)
{
    const uint16_t adcData = adc->dataReg[GetChannel(adc->commsReg)]; 
    const uint8_t  msb = (adcData >> 8U) & 0xFF;
    const uint8_t  lsb = adcData & 0xFF;

    buf[0U] = msb;
    buf[1U] = lsb;
}

// sets DRDY -> 1 meaning busy
static void SetCommsRegBusy(uint8_t *commsReg)
{
    bitSet(*commsReg, DRDY_BIT);
}

// sets DRDY -> 0 meaning ready
static void ClearCommsRegBusy(uint8_t *commsReg)
{
    bitClear(*commsReg, DRDY_BIT);
}

// Returns the busy status of the comms register
static bool IsCommsRegBusy(uint8_t *commsReg)
{
    return bitRead(*commsReg, DRDY_BIT);
}

/*
 Manages the data register of the adc depending on the time that has elapsed
 since the last measurement. Only when the elapsed time exceeds the conversion
 time do we need to update the register with new data and reset the timer.
*/
static void UpdateMeasurementStatus(void)
{
    // conversion time is time taken to measure and convert voltage to data
    if ((elapsedTime > conversionTime) && !triggerTimeout)
    {
        // data has been converted so we can clear busy bit
        ClearCommsRegBusy(&mx7705Adc.commsReg);
        // and put the ADC data into the data register
        mx7705Adc.dataReg[GetChannel(mx7705Adc.commsReg)] = adcInput;
        // reset timer to prepare for the next measurement
        elapsedTime = 0U;
    }
    else
    {
        // Timers incremented each time spi is called for polling purposes
        mockMillis += roundtripTime;  // time returned to source code from calls to millis()
        elapsedTime += roundtripTime; // timer for updating data register.
        // data not converted yet
        SetCommsRegBusy(&mx7705Adc.commsReg);
    }
}

// write the data from the input buffer into the io registers
static void TeardownMockSpiConnection(const SpiSettings_t *settings)
{
    CheckSpiSettings(settings);

    const RegisterSelection_t regRequest = 
        GetRequestedRegister(mx7705Adc.commsReg);

    if (IsReadOp(mx7705Adc.commsReg))
    {
        // Command done so reset the register.
        mx7705Adc.commsReg = 0U;
    }
    else //writeop, then data is copied into the relevant reg from inputBuffer
    {
        const uint8_t ch = GetChannel(mx7705Adc.commsReg);
        switch (regRequest)
        {
            case CommsReg:
                /*
                 Write requested into comms reg - copy from bytes captured in the
                 mock spi input buffer
                 Is this accurate. If several bytes are written what will end up in the comms reg?
                */
                mx7705Adc.commsReg = inputBuffer[0U]; 

                // No need to reset the register as we've just written in a command.
                break;
            case SetupReg:
                /*write requested into other reg - copy from byte(s) captured in the
                mock spi input buffer*/
                mx7705Adc.setupReg[ch] = inputBuffer[0U];
                // Command done so reset the register.
                mx7705Adc.commsReg = 0U;
                break;
            case ClockReg:
                mx7705Adc.clockReg[ch] = inputBuffer[0U];
                // Command done so reset the register.
                mx7705Adc.commsReg = 0U;
                break;
            case DataReg:
                // You can write to the data register but this is ignored.
                mx7705Adc.dataReg[ch] = 
                    (uint16_t)((inputBuffer[0U] << 8U) | inputBuffer[1U]);
                // Command done so reset the register.
                mx7705Adc.commsReg = 0U;
                break;
            default:
                FAIL("Register request not implemented.");
                break;
        }
    }
}

// Loads data into the spi output buffer if a read is requested in the comms reg
static void SetupMockSpiConnection(const SpiSettings_t *settings)
{
    CheckSpiSettings(settings);
    ResetMockSpiBuffers();

    const RegisterSelection_t regRequest = GetRequestedRegister(mx7705Adc.commsReg);

    /* 
     If it's a readOp, then load data into the required spi output buffer. 
     
     Mocking of data ready (DRDY) bit 7 of comms reagister is implemented here. 
     Ready bit set to 1 when conversion has finished. And data is ready to be 
     read from the data reg. 

     Read error is mocked by preventing data being copied into spi output.
     */

    if (IsReadOp(mx7705Adc.commsReg) && !mockReadError)
    {
        const uint8_t ch = GetChannel(mx7705Adc.commsReg);
        switch (regRequest)
        {
            case CommsReg:

                // Increment timers and update data register if needed.
                UpdateMeasurementStatus();

                /* Data copied into the mock spi output buffer to be transferred */
                outputBuffer[0U] = mx7705Adc.commsReg;  // only 1 byte
                break;
            case DataReg:
                /*
                 Data only copied from data register when conversion has
                 happened. This is signalled by the the DRDY bit in the comms reg.
                 */
                if (!IsCommsRegBusy(&mx7705Adc.commsReg))
                {
                    LoadAdcDataIntoByteBuf(outputBuffer, &mx7705Adc);
                }
                /*otherwise adc is busy and there's no new data to read. If a
                read on the data register is requested then the old data (from a
                previous conversion) will be returned*/
                break;
            case SetupReg:
                outputBuffer[0U] = mx7705Adc.setupReg[ch];
                break;
            case ClockReg:
                outputBuffer[0U] = mx7705Adc.clockReg[ch];
                break;
            default:
                // default case for other reg requests that haven't been implemented
                FAIL("Register request not implemented.");
                break;
        }
    }
}

/*******************************************************************************
 * Mock function implementations for tests
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
static void MockForMX7705Init(uint8_t channel)
{
    MockForMX7705Write(RequestRegWrite(ClockReg, channel));
    MockForMX7705Write(SetClockSettings(true, false, false, 1U));
    MockForMX7705Write(RequestRegWrite(SetupReg, channel));
    MockForMX7705Write(SetSetupSettings(SelfCalibMode, 0U, true, false, false));
    MockForMX7705Write(RequestRegRead(ClockReg, channel));
    MockForMX7705Read();
    MockForMX7705Write(RequestRegRead(SetupReg, channel));
    MockForMX7705Read();
}

// Mocks for calling read data function
static void MockForMX7705DataRead(uint8_t channel)
{   
    MockForMX7705Write(RequestRegRead(DataReg, channel));
    MockForMX7705Read16Bit();        
}

// Mocks for polling comms reg on the specified channel
static void MockForMX7705Polling(uint8_t channel)
{
    const int nPolls = triggerTimeout ?
        TIMEOUT_MS / roundtripTime:
        conversionTime / roundtripTime;

    mock().expectOneCall("millis");    
    /* 
     Mocks for polling comms reg - only poll within timeout or conversion time.
     Note that another poll is expected since we have a do while loop - poll
     happens before time is checked.
    */
    for (int i = 0; (i <= nPolls + 1); i++)
    {
        mock().expectOneCall("millis");    
        MockForMX7705Write(RequestRegRead(CommsReg, channel));
        MockForMX7705Read();        
    }
}    

// Mocks for calling get gain  
static void MockForMX7705GetGain(uint8_t channel)
{
    MockForMX7705Write(RequestRegRead(SetupReg, channel));
    // MockForMX7705Write(MX7705_REQUEST_SETUP_READ_CH0);
    MockForMX7705Read();
}

/*
 Mocks for calling get gain - note that the setup register is read, altered
 and then rewritten when we get gain so we need access to the adc object.
*/
static void MockForMX7705SetGain(uint8_t channel, uint8_t newSetupReg)
{
    MockForMX7705Write(RequestRegRead(SetupReg, channel));
    MockForMX7705Read();
    MockForMX7705Write(RequestRegWrite(SetupReg, channel));
    MockForMX7705Write(newSetupReg);
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

        // TODO: adc input needs channel information
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
    
    MockForMX7705Init(channel);

    MX7705_Init(pinNum, channel);

    const uint8_t clockRegExp = MX7705_WRITE_CLOCK_SETTINGS;
    const uint8_t clockRegAct = mx7705Adc.clockReg[channel];
    CHECK_EQUAL(clockRegExp, clockRegAct);

    const uint8_t setupRegExp = MX7705_WRITE_SETUP_INIT;
    const uint8_t setupRegAct = mx7705Adc.setupReg[channel];
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
    
    MockForMX7705Init(channel);

    MX7705_Init(pinNum, channel);

    const uint8_t clockRegExp = MX7705_WRITE_CLOCK_SETTINGS;
    const uint8_t clockRegAct = mx7705Adc.clockReg[channel];
    CHECK_EQUAL(clockRegExp, clockRegAct);

    const uint8_t setupRegExp = MX7705_WRITE_SETUP_INIT;
    const uint8_t setupRegAct = mx7705Adc.setupReg[channel];
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
    
    MockForMX7705Init(channel);

    // allows us to mock a read fail which triggers verification fail
    mockReadError = true;
    // call function under test
    MX7705_Init(pinNum, channel);

    CHECK_EQUAL(pinNum, mx7705SpiSettings.chipSelectPin);

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
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // Mocks needed to call function under test
    MockForMX7705Polling(channel);
    MockForMX7705DataRead(channel);

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
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // Check that there is no data in the Adc data reg to begin with
    MockForMX7705Polling(channel);
    MockForMX7705DataRead(channel);
    CHECK_EQUAL(0U, adcInput);
    CHECK_EQUAL(adcInput, MX7705_ReadData(channel));
    // expect error condition due to measurement timeout
    CHECK_EQUAL(false, MX7705_GetError());     

    // Set an input to the adc and call function under test
    MockForMX7705Polling(channel);
    MockForMX7705DataRead(channel);
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
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // mock input
    adcInput = 2456U;
    
    // Set timout error flag
    triggerTimeout = true;
    
    // Mock/expects function calls
    MockForMX7705Polling(channel);
    
    // Check data register is not changed by reading data due to timeout error
    const uint16_t dataToExpect = mx7705Adc.dataReg[channel];

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
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);
    // Initialise should be successful and no error raised
    CHECK_EQUAL(false, MX7705_GetError()); 

    // mock input
    adcInput = 2456U;
    
    // Set timout error flag
    triggerTimeout = true;
    
    // Mock/expects function calls
    MockForMX7705Polling(channel);
    
    // Check data register is not changed by reading data due to timeout error
    const uint16_t dataToExpect = mx7705Adc.dataReg[channel];

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
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);

    // Read the initial gain. Should match the settings stored in the setup reg
    const uint8_t setupRegInitial = mx7705Adc.setupReg[channel];
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
    MockForMX7705SetGain(channel, setupRegExpected);

    // Call to set gain
    MX7705_SetGain(gainExpected, channel);
    
    // Now call get gain again to check that the gain has actually been written.

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);

    // check the gain returned matches gain requested
    gainActual = MX7705_GetGain(channel);
    CHECK_EQUAL(gainExpected, gainActual);

    // check that the setup register matches expectations
    const uint8_t setupRegActual = mx7705Adc.setupReg[channel];
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
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);

    // Get the gain and setup reg data to begin with...
    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    const uint8_t setupRegInitial = mx7705Adc.setupReg[channel];
    const uint8_t gainInitial = MX7705_GetGain(channel);

    // now request gain outside range
    const uint8_t gainRequested = PGA_MASK + 1U;
    MX7705_SetGain(gainRequested, channel);
    
    // Get the gain and setup reg data again to compare against...
    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    // check the gain returned matches gain requested
    const uint8_t setupRegFinal = mx7705Adc.setupReg[channel];
    const uint8_t gainFinal = MX7705_GetGain(channel);

    CHECK_EQUAL(gainFinal, gainInitial);
    CHECK_EQUAL(setupRegFinal, setupRegInitial);

    // check function calls
    mock().checkExpectations();
}

// Test for getting and setting gain on channel 0.
TEST(MX7705TestGroup, SetGetGainChannelZero)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);

    // Read the initial gain. Should match the settings stored in the setup reg
    const uint8_t setupRegInitial = mx7705Adc.setupReg[channel];
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
    MockForMX7705SetGain(channel, setupRegExpected);
    // Call to set gain
    MX7705_SetGain(gainExpected, channel);
    
    // Now call get gain again to check that the gain has actually been written.

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    // check the gain returned matches gain requested
    gainActual = MX7705_GetGain(channel);
    CHECK_EQUAL(gainExpected, gainActual);

    // check that the setup register matches expectations
    const uint8_t setupRegActual = mx7705Adc.setupReg[channel];
    CHECK_EQUAL(setupRegExpected, setupRegActual);

    // check function calls
    mock().checkExpectations();
}

/*
 Test for getting and setting gain on channel 0 outside allowed range. Maximum
 allowed gain setting is 7. Here we set to 8. Gain should not change. Command
 is ignored.
*/
TEST(MX7705TestGroup, SetGetGainOutsideRangeCommandIgnoredChannelZero)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);

    // Get the gain and setup reg data to begin with...
    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    const uint8_t setupRegInitial = mx7705Adc.setupReg[channel];
    const uint8_t gainInitial = MX7705_GetGain(channel);

    // now request gain outside range
    const uint8_t gainRequested = PGA_MASK + 1U;
    MX7705_SetGain(gainRequested, channel);
    
    // Get the gain and setup reg data again to compare against...
    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    // check the gain returned matches gain requested
    const uint8_t setupRegFinal = mx7705Adc.setupReg[channel];
    const uint8_t gainFinal = MX7705_GetGain(channel);

    CHECK_EQUAL(gainFinal, gainInitial);
    CHECK_EQUAL(setupRegFinal, setupRegInitial);

    // check function calls
    mock().checkExpectations();
}

/*
 Test for incrementing gain index on channel zero. Calling function under test
 should mean that the adc gain index is increased by one step.
*/
TEST(MX7705TestGroup, IncrementGainChannelZero)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);

    // Read the initial gain. Should match the settings stored in the setup reg
    const uint8_t setupRegInitial = mx7705Adc.setupReg[channel];
    uint8_t gainActual = MX7705_GetGain(channel);
    uint8_t gainExpected = bitExtract(setupRegInitial, PGA_MASK, PGA_OFFSET);
    CHECK_EQUAL(gainExpected, gainActual);

    // Work out the expected setup register after changing gain.
    uint8_t setupRegExpected = setupRegInitial;
    // increment gain to be written back into the setup reg.
    gainExpected++;
    // write in the new gain
    bitInsert(setupRegExpected, gainExpected, PGA_MASK, PGA_OFFSET);
    // mocks for calling increment gain (get then set gain)
    MockForMX7705GetGain(channel);
    MockForMX7705SetGain(channel, setupRegExpected);
    // Call to increment gain
    MX7705_IncrementGain(channel);
    
    // Now call get gain again to check that the gain has actually been written.

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    // check the gain returned matches gain requested
    gainActual = MX7705_GetGain(channel);
    CHECK_EQUAL(gainExpected, gainActual);

    // check that the setup register matches expectations
    const uint8_t setupRegActual = mx7705Adc.setupReg[channel];
    CHECK_EQUAL(setupRegExpected, setupRegActual);

    // check function calls
    mock().checkExpectations();
}

/*
 Test for decrementing gain index on channel zero. Calling function under test
 should mean that the adc gain index is decreased by one step.
*/
TEST(MX7705TestGroup, DecrementGainChannelZero)
{
    const uint8_t pinNum = 1U;
    const uint8_t channel = 0U;
    const uint8_t initGain = 3U;
    
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    MockForMX7705Init(channel);
    
    MX7705_Init(pinNum, channel);
    /* Note that gain is initialised to idx 0. So lets change to 3 so it can be
    decremented. Change setup reg direcly. */
    bitInsert(mx7705Adc.setupReg[channel], initGain, PGA_MASK, PGA_OFFSET);

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);

    // Read the initial gain. Should match the settings stored in the setup reg
    const uint8_t setupRegInitial = mx7705Adc.setupReg[channel];
    uint8_t gainActual = MX7705_GetGain(channel);
    uint8_t gainExpected = initGain;
    // check that the gain we get back was what we set
    CHECK_EQUAL(gainExpected, gainActual);

    // Work out the expected setup register after changing gain.
    uint8_t setupRegExpected = setupRegInitial;
    // decrement gain to be written back into the setup reg.
    gainExpected--;
    // write in the new gain
    bitInsert(setupRegExpected, gainExpected, PGA_MASK, PGA_OFFSET);
    // mocks for calling increment gain (get then set gain)
    MockForMX7705GetGain(channel);
    MockForMX7705SetGain(channel, setupRegExpected);
    // Call to increment gain
    MX7705_DecrementGain(channel);
    
    // Now call get gain again to check that the gain has actually been written.

    // Mocks for calling get gain
    MockForMX7705GetGain(channel);
    // check the gain returned matches gain requested
    gainActual = MX7705_GetGain(channel);
    CHECK_EQUAL(gainExpected, gainActual);

    // check that the setup register matches expectations
    const uint8_t setupRegActual = mx7705Adc.setupReg[channel];
    CHECK_EQUAL(setupRegExpected, setupRegActual);

    // check function calls
    mock().checkExpectations();
}