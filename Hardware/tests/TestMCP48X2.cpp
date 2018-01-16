// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "MCP48X2.h"
#include "MCP48X2Private.h"

// support
#include "Arduino.h"       // arduino function prototypes - implemented MockAduino.c
#include "Config.h"
#include "MockArduino.h"   // mockDigitalPins, mockMillis, private variables
#include "MockSpiCommon.h" // Mock spi interface
#include "SpiCommon.h"     // spi function prototypes - mocks implemented here.
#include <string.h>

#define BUFFER_SIZE        (16U) // mock spi buffer size in bytes 

typedef struct mockDacChannel_s {
    uint16_t     output;
    shdnSelect_t shdnMode;
} mockDacChannel_t;

typedef struct mockDacState_s {
    uint16_t         writeReg;     // unidirectional write register
    gainSelect_t     gainMode;     // on/off = low/high
    mockDacChannel_t chA;          // channel A output
    mockDacChannel_t chB;          // channel B output
} mockDacState_t;

// DAC object storing settings and status of the whole device
static mockDacState_t mockDac;

/* Mock spi buffers used in transferring data
TODO: move to mockSpiState defined in MockSpiCommon.cpp */
static uint8_t inputBuffer[BUFFER_SIZE];
static uint8_t outputBuffer[BUFFER_SIZE];
// counts number of bytes transferred to/from the Spi buffer
static uint8_t counter;

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
// reset the buffers and counter
static void ResetMockSpiBuffers(void)
{
    memset(inputBuffer, 0U, BUFFER_SIZE);
    memset(outputBuffer, 0U, BUFFER_SIZE);
    counter = 0U;    
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

// write the data from the input (MCU->peripheral) buffer into the io registers
static void TeardownMockSpiConnection(const SpiSettings_t *settings)
{
    CheckSpiSettings(settings);
    // dac command is given by the first two bytes written over SPI bus
    const uint16_t dacCommand = (inputBuffer[0] << 8U) | (inputBuffer[1]);
    printf("dacCommand = %d\n", dacCommand);    
    // Interpret the command and use it to set the output of the device
    const chSelect_t ch = bitRead(dacCommand, CH_SELECT_BIT) ? chBSelect : chASelect;
    const gainSelect_t gain = bitRead(dacCommand, GAIN_SELECT_BIT) ? lowGain : highGain;
    const shdnSelect_t shdn = bitRead(dacCommand, SHDN_BIT) ? shdnOff : shdnOn;
    const uint16_t output = bitExtract(dacCommand, DATA_MASK, DATA_OFFSET);

    mockDac.writeReg = dacCommand;
    mockDac.gainMode = gain;
    switch (ch)
    {
        case chASelect:
            mockDac.chA.shdnMode = shdn;
            mockDac.chA.output = output;
            break;
        case chBSelect:
            mockDac.chB.shdnMode = shdn;
            mockDac.chB.output = output;
            break;
        default:
            break;
    }

}

/*
 Loads data into the spi output (peripheral->MCU) buffer if a read is requested
 in the comms reg.
*/
static void SetupMockSpiConnection(const SpiSettings_t *settings)
{
    CheckSpiSettings(settings);
    ResetMockSpiBuffers();
#if 0

    const RegisterSelection_t regRequest = GetRequestedRegister(mx7705Adc.commsReg);
    printf("regRequest = %u channel = %u\n",
        (unsigned int)regRequest, GetChannel(mx7705Adc.commsReg));

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
                
                printf("mockMillis %lu elapsedTime %u\n", mockMillis, elapsedTime);

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
#endif
}

/*******************************************************************************
 * Mock function implementations for tests
 ******************************************************************************/
// Mocks needed for writing data into mock spi reg
static void MockForMcp48x2Write(uint16_t data)
{
    const uint8_t msb = (data >> 8U) & 0xFF;
    const uint8_t lsb = data & 0xFF;

    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &mcp48x2SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", msb);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", lsb);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &mcp48x2SpiSettings);
}

// Contains commands for mocks for calls to init function
static void MockForMcp48x2Init(void)
{
    MockForMcp48x2Write(MCP48X2_GetDacCommand(chASelect,
                                              lowGain,
                                              shdnOff,
                                              0U));

    MockForMcp48x2Write(MCP48X2_GetDacCommand(chBSelect,
                                              lowGain,
                                              shdnOff,
                                              0U));
}

/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group - all tests share common setup/teardown
TEST_GROUP(Mcp48x2TestGroup)
{
    void setup(void)
    {
        InitialiseMockSpiBus(&mcp48x2SpiSettings);
        

        SpiTransferByte_Callback = &TransferMockSpiData;
        OpenSpiConnection_Callback = &SetupMockSpiConnection;
        CloseSpiConnection_Callback = &TeardownMockSpiConnection;
        
    }

    void teardown(void)
    {
        mock().clear();
    }
};

/*
 Test for initialising MCP48X2 device on pin 2. After calling the init function
 both channels should be active and set to 0.
 */
TEST(Mcp48x2TestGroup, InitOk)
{
    const uint8_t pinNum = 2U;
    CHECK_EQUAL(0xFF, mcp48x2SpiSettings.chipSelectPin);
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);

    MockForMcp48x2Init();

    MCP48X2_Init(pinNum);

    CHECK_EQUAL(lowGain, mockDac.gainMode);
    CHECK_EQUAL(0U, mockDac.chA.output);
    CHECK_EQUAL(shdnOff, mockDac.chA.shdnMode);
    CHECK_EQUAL(0U, mockDac.chB.output);
    CHECK_EQUAL(shdnOff, mockDac.chB.shdnMode);

    // check function calls
    mock().checkExpectations();
}
#if 1
/*
 Test for setting the output of both channels of the dac and turn the output on
 (turn shutdown off).
 */
TEST(Mcp48x2TestGroup, SetChAandChBWithOutputOnLowGain)
{
    const uint8_t pinNum = 2U;
    CHECK_EQUAL(0xFF, mcp48x2SpiSettings.chipSelectPin);
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);

    // initialise the Dac
    MockForMcp48x2Init();
    MCP48X2_Init(pinNum);

    // Set arbitrary output code to both channels
    const uint16_t outputExpected = 116U;
    MockForMcp48x2Write(
        MCP48X2_GetDacCommand(chASelect, lowGain, shdnOff, outputExpected));

    MCP48X2_Output(outputExpected, 'a');

    MockForMcp48x2Write(
        MCP48X2_GetDacCommand(chBSelect, lowGain, shdnOff, outputExpected));

    MCP48X2_Output(outputExpected, 'b');

    CHECK_EQUAL(lowGain, mockDac.gainMode);
    CHECK_EQUAL(outputExpected, mockDac.chA.output);
    CHECK_EQUAL(shdnOff, mockDac.chA.shdnMode);
    CHECK_EQUAL(outputExpected, mockDac.chB.output);
    CHECK_EQUAL(shdnOff, mockDac.chB.shdnMode);

    // check function calls
    mock().checkExpectations();

}
#endif
/*
 Test for setting the output of both channels of the dac and with the output on
 and at high gain.
 */
TEST(Mcp48x2TestGroup, SetChAandChBWithOutputOnHighGain)
{
    const uint8_t pinNum = 4U;
    CHECK_EQUAL(0xFF, mcp48x2SpiSettings.chipSelectPin);
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);

    // initialise the mock dac object
    MockForMcp48x2Init();
    MCP48X2_Init(pinNum);

    // Set arbitrary output code to both channels and gain
    const uint16_t outputExpected = 78U;
    MCP48X2_SetGain('h');
    
    MockForMcp48x2Write(
        MCP48X2_GetDacCommand(chASelect, highGain, shdnOff, outputExpected));

    MCP48X2_Output(outputExpected, 'a');

    MockForMcp48x2Write(
        MCP48X2_GetDacCommand(chBSelect, highGain, shdnOff, outputExpected));

    MCP48X2_Output(outputExpected, 'b');

    CHECK_EQUAL(highGain, mockDac.gainMode);
    CHECK_EQUAL('h', MCP48X2_GetGain());

    CHECK_EQUAL(outputExpected, mockDac.chA.output);
    CHECK_EQUAL(shdnOff, mockDac.chA.shdnMode);
    CHECK_EQUAL(outputExpected, mockDac.chB.output);
    CHECK_EQUAL(shdnOff, mockDac.chB.shdnMode);

    // check function calls
    mock().checkExpectations();

}