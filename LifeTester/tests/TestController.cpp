// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "Controller.h"
#include "Controller_Private.h"

// support
#include "Config.h"
#include "Wire.h"
#include "Arduino.h"
#include "MockArduino.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "StateMachine.h"
#include "StateMachine_Private.h"
#include <string.h> //memset, memcpy

#define DATA_SEND_SIZE  (13U)  // size of data sent for single channel

static LifeTester_t *mockLifeTesterA;
static LifeTester_t *mockLifeTesterB;
static DataBuffer_t mockRxBuffer;  // data received by device from master see Wire.cpp

/*******************************************************************************
* PRIVATE FUNCTION IMPLEMENTATIONS
*******************************************************************************/
static bool IsEmpty(DataBuffer_t const *const buf)
{
    return !(buf->head < buf->tail);
}

static uint8_t ReadUint8(DataBuffer_t *const buf)
{
    uint8_t retVal = 0U;
    if (!IsEmpty(buf))
    {
        retVal = buf->d[buf->head];
        buf->head++;
    }
    return retVal;
}

static uint16_t ReadUint16(DataBuffer_t *const buf)
{
    const uint8_t lsb = ReadUint8(buf);
    const uint8_t msb = ReadUint8(buf);
    return (uint16_t)(lsb | (msb << 8U));
}

static uint32_t ReadUint32(DataBuffer_t *const buf)
{
    uint32_t retVal = 0U;
    for (int i = 0; i < sizeof(uint32_t); i++)
    {
        retVal |= (ReadUint8(buf) << (i * 8U));
    }
    return retVal;
}

/*******************************************************************************
* MOCK FUNCTION IMPLEMENTATIONS FOR TEST
*******************************************************************************/
// intantiates the wire interface object
TwoWire::TwoWire()
{
}

// Mocks needed for reading data in update function
size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    mock().actualCall("TwoWire::write")
        .withParameter("data", (uint8_t *)data)
        .withParameter("quantity", quantity);

    memcpy(transmitBuffer.d, data, quantity);
    transmitBuffer.tail += quantity;
    return (size_t)mock().unsignedIntReturnValue();
}

size_t TwoWire::write(uint8_t data)
{
    mock().actualCall("TwoWire::write")
        .withParameter("data", data);

    return (size_t)mock().unsignedIntReturnValue();
}

int TwoWire::read(void)
{
    mock().actualCall("TwoWire::read");
    return mock().intReturnValue();
}

int TwoWire::available(void)
{
    mock().actualCall("TwoWire::available");
    return mock().intReturnValue();    
}

void StateMachine_Reset(LifeTester_t *const lifeTester)
{
    mock().actualCall("StateMachine_Reset")
        .withParameter("lifeTester", lifeTester);
}

// instance of TwoWire visible from tests and source via external lilnkage in header
TwoWire Wire = TwoWire();

/*******************************************************************************
* MOCKS FOR TESTS
*******************************************************************************/
static void MocksForReadTempData(uint16_t mockTemp)
{
    mock().expectOneCall("TempGetRawData")
        .andReturnValue(mockTemp);    
}

static void MocksForAnalogRead(int16_t mockVal)
{
    mock().expectOneCall("analogRead")
        .withParameter("pin", LIGHT_SENSOR_PIN)
        .andReturnValue(mockVal);
}

static void MocksForWireWrite(uint8_t byteToSend)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", byteToSend);
}

static void MocksForWireTransmitArray(uint8_t numBytes)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", transmitBuffer.d)
        .withParameter("quantity", numBytes);
}

static void MocksForWireRead(uint8_t byteReceived)
{
    mock().expectOneCall("TwoWire::read")
        .andReturnValue(byteReceived);
}

static void MocksForCommsLedOn(void)
{
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", COMMS_LED_PIN)
        .withParameter("value", HIGH);
}

static void MocksForCommsLedOff(void)
{
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", COMMS_LED_PIN)
        .withParameter("value", LOW);
}

/*******************************************************************************
* HELPERS
*******************************************************************************/
static uint8_t SetCtrlReadCmd(bool ch)
{
    uint8_t cmd = 0U;
    bitWrite(cmd, CH_SELECT_BIT, ch);
    return cmd;
}

static uint8_t SetCtrlWriteCmd(bool ch, ControllerCommand_t ctrlCmd)
{
    uint8_t cmd = 0U;
    bitInsert(cmd, ctrlCmd, COMMAND_MASK, COMMAND_OFFSET);
    bitWrite(cmd, CH_SELECT_BIT, ch);
    bitSet(cmd, RW_BIT);
    return cmd;  
}

const uint32_t timeExpectedA = 23432;
const uint16_t vExpectedA = 34U;
const uint16_t iExpectedA = 2345U;
const uint16_t tempExpectedA = 924U;
const uint16_t adcReadExpectedA = 234U;
const ErrorCode_t errorExpectedA = lowCurrent;
const uint32_t timeExpectedB = 23432;
const uint16_t vExpectedB = 34U;
const uint16_t iExpectedB = 5245U;
const uint16_t tempExpectedB = 424U;
const uint16_t adcReadExpectedB = 134U;
const ErrorCode_t errorExpectedB = invalidScan;

static void SetExpectedLtDataA(LifeTester_t *const lifeTester)
{   
    lifeTester->data.vActive = &lifeTester->data.vThis;
    lifeTester->data.iActive = &lifeTester->data.iThis;
    lifeTester->timer = timeExpectedA;
    lifeTester->data.vThis = vExpectedA;
    lifeTester->data.iThis = iExpectedA;
    lifeTester->error = errorExpectedA;
}

static void SetExpectedLtDataB(LifeTester_t *const lifeTester)
{   
    lifeTester->data.vActive = &lifeTester->data.vThis;
    lifeTester->data.iActive = &lifeTester->data.iThis;
    lifeTester->timer = timeExpectedB;
    lifeTester->data.vThis = vExpectedB;
    lifeTester->data.iThis = iExpectedB;
    lifeTester->error = errorExpectedB;
}

/*******************************************************************************
 * UNIT TESTS
 ******************************************************************************/
TEST_GROUP(ControllerTestGroup)
{
    void setup(void)
    {
        ResetBuffer(&transmitBuffer);
        mock().disable();
        pinMode(COMMS_LED_PIN, OUTPUT);
        const LifeTester_t lifeTesterInit = {
            {chASelect, 0U},    // io
            Flasher(LED_A_PIN), // led
            {0},                // data
            0U,                 // timer
            ok,                 // error
            NULL
        };
        mock().enable();
        // Copy to a static variable for tests to work on
        static LifeTester_t dataForTestA = lifeTesterInit;
        static LifeTester_t dataForTestB = lifeTesterInit;
        // Need to copy the data every time. A static is only initialised once.
        memcpy(&dataForTestA, &lifeTesterInit, sizeof(LifeTester_t));
        memcpy(&dataForTestB, &lifeTesterInit, sizeof(LifeTester_t));
        // access data through pointers
        mockLifeTesterA = &dataForTestA;
        mockLifeTesterB = &dataForTestB;
        cmdReg = 0U;
        bitSet(cmdReg, RDY_BIT);
    }

    void teardown(void)
    {
        mock().clear();
    }
};

TEST(ControllerTestGroup, DataCopiedToTransmitBufferOk)
{
    SetExpectedLtDataA(mockLifeTesterA);
    MocksForReadTempData(tempExpectedA);
    MocksForAnalogRead(adcReadExpectedA);
    WriteDataToTransmitBuffer(mockLifeTesterA);
    PrintBuffer(&transmitBuffer);
    CHECK_EQUAL(timeExpectedA, ReadUint32(&transmitBuffer));
    CHECK_EQUAL(vExpectedA, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(iExpectedA, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(tempExpectedA, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(adcReadExpectedA, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(errorExpectedA, ReadUint8(&transmitBuffer));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, CheckSumCalcultesOk)
{
    DataBuffer_t b;
    const uint8_t checkSumExpected = 12U;
    WriteUint8(&b, 4U);
    WriteUint8(&b, 1U);
    WriteUint8(&b, 0U);
    WriteUint8(&b, 7U);
    const uint8_t checkSumActual = CheckSum(&b);
    CHECK_EQUAL(checkSumExpected, checkSumActual);
}

TEST(ControllerTestGroup, GetCmdRegChAOk)
{
    uint8_t cmd = SetCtrlReadCmd(LIFETESTER_CH_A);
    cmdReg = cmd;  // artificially set the cmd reg
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, cmdReg);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    MocksForCommsLedOn();
    MocksForWireWrite(cmd);
    MocksForCommsLedOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, cmdReg);
    mock().checkExpectations();
}

TEST(ControllerTestGroup, GetCmdRegChBOk)
{
    uint8_t cmd = SetCtrlReadCmd(LIFETESTER_CH_B);
    cmdReg = cmd;
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, cmdReg);
    Controller_ConsumeCommand(mockLifeTesterB, mockLifeTesterB);
    MocksForCommsLedOn();
    MocksForWireWrite(cmd);
    MocksForCommsLedOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, cmdReg);
    mock().checkExpectations();
}

TEST(ControllerTestGroup, ResetChannelAOk)
{
    uint8_t newCmd = SetCtrlWriteCmd(LIFETESTER_CH_A, Reset);
    MocksForCommsLedOn();
    MocksForWireRead(newCmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(newCmd, cmdReg);
    mock().expectOneCall("StateMachine_Reset")
        .withParameter("lifeTester", mockLifeTesterA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    MocksForCommsLedOn();
    // get the command back again
    uint8_t cmdExpected = newCmd;
    bitClear(cmdExpected, RW_BIT);  // set to read mode
    bitSet(cmdExpected, RDY_BIT);   // done
    MocksForWireWrite(cmdExpected);
    MocksForCommsLedOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdExpected, cmdReg);
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChANotReadyCmdReturnsCmdReg)
{   
    uint8_t newCmd = SetCtrlWriteCmd(LIFETESTER_CH_A, GetData);
    MocksForCommsLedOn();
    MocksForWireRead(newCmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(newCmd, cmdReg);
    // Data requested before command consumed - not ready
    uint8_t cmdExpected = newCmd;
    MocksForCommsLedOn();
    MocksForWireWrite(cmdExpected);
    MocksForCommsLedOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdExpected, cmdReg);
    mock().checkExpectations();
}

#if 0
TEST(ControllerTestGroup, RequestDataFromChANotReadyHackDataRdyCmdBits)
{   
    uint8_t cmdExpected = SetCtrlCmd(LIFETESTER_CH_A, GetData);
    // user tries to force data ready and cmd done bits
    uint8_t cmdActual = cmdExpected;
    bitSet(cmdActual, DATA_RDY_BIT);
    bitSet(cmdActual, CMD_DONE_BIT);
    MocksForCommsLedOn();
    MocksForWireRead(cmdActual);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmdExpected, cmdReg);
    // Data requested before command consumed - not ready
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    MocksForWireWrite(cmdExpected);
    Controller_RequestHandler();
    CHECK_EQUAL(cmdExpected, cmdReg);
    CHECK(!bitRead(cmdReg, DATA_RDY_BIT));
    CHECK(!bitRead(cmdReg, CMD_DONE_BIT));
}

TEST(ControllerTestGroup, RequestDataFromChBNotReadyHackDataRdyCmdBits)
{   
    uint8_t cmdExpected = SetCtrlCmd(LIFETESTER_CH_B, GetData);
    // user tries to force data ready and cmd done bits
    uint8_t cmdActual = cmdExpected;
    bitSet(cmdActual, DATA_RDY_BIT);
    bitSet(cmdActual, CMD_DONE_BIT);
    MocksForCommsLedOn();
    MocksForWireRead(cmdActual);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmdExpected, cmdReg);
    // Data requested before command consumed - not ready
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    MocksForWireWrite(cmdExpected);  // just returns the comms reg
    Controller_RequestHandler();
    CHECK_EQUAL(cmdExpected, cmdReg);
    CHECK(!bitRead(cmdReg, DATA_RDY_BIT));
    CHECK(!bitRead(cmdReg, CMD_DONE_BIT));
}

TEST(ControllerTestGroup, RequestDataFromChAReady)
{   
    SetExpectedLtDataA(mockLifeTesterA);
    SetExpectedLtDataB(mockLifeTesterB);
    uint8_t cmd = SetCtrlCmd(LIFETESTER_CH_A, GetData);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, cmdReg);
    MocksForReadTempData(tempExpectedA);
    MocksForAnalogRead(adcReadExpectedA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    CHECK(bitRead(cmdReg, DATA_RDY_BIT));
    CHECK(!bitRead(cmdReg, CMD_DONE_BIT));
    // Read comms reg to check that data is ready
    cmd = SetCtrlCmd(LIFETESTER_CH_A, GetcmdReg);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    Controller_ReceiveHandler(nBytesSent);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    uint8_t cmdExpected = cmd;
    bitSet(cmdExpected, DATA_RDY_BIT);
    MocksForCommsLedOn();
    MocksForWireWrite(cmdExpected);
    MocksForCommsLedOff();
    Controller_RequestHandler();
    /*
    expected and actual commands returned don't agree. data ready/cmd done are reset
    command mode is now getcmdReg rather than get data.
    
    */
    #if 0
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    nBytesSent = DATA_SEND_SIZE;
    MocksForWireTransmitArray(nBytesSent);
    Controller_RequestHandler();
    CHECK_EQUAL(cmdExpected, cmdReg);
    CHECK(!bitRead(cmdReg, DATA_RDY_BIT));
    CHECK(!bitRead(cmdReg, CMD_DONE_BIT));
    #endif
}
#endif