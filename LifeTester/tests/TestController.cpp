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
static void ExpectReadTempAndReturn(uint16_t mockTemp)
{
    mock().expectOneCall("TempGetRawData")
        .andReturnValue(mockTemp);    
}

static void ExpectAnalogReadAndReturn(int16_t mockVal)
{
    mock().expectOneCall("analogRead")
        .withParameter("pin", LIGHT_SENSOR_PIN)
        .andReturnValue(mockVal);
}

static void ExpectTransmitByte(uint8_t byteToSend)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", byteToSend);
}

static void ExpectSendTransmitBuffer(DataBuffer_t *const buf)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", buf->d)
        .withParameter("quantity", NumBytes(buf));
}

static void ExpectReceiveByte(uint8_t byteReceived)
{
    mock().expectOneCall("TwoWire::read")
        .andReturnValue(byteReceived);
}

static void ExpectCommsLedSwitchOn(void)
{
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", COMMS_LED_PIN)
        .withParameter("value", HIGH);
}

static void ExpectCommsLedSwitchOff(void)
{
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", COMMS_LED_PIN)
        .withParameter("value", LOW);
}

/*******************************************************************************
* HELPERS
*******************************************************************************/
static uint8_t GetCtrlReadCmd(bool ch, ControllerCommand_t ctrlCmd)
{
    uint8_t cmd = 0U;
    bitInsert(cmd, ctrlCmd, COMMAND_MASK, COMMAND_OFFSET);
    bitWrite(cmd, CH_SELECT_BIT, ch);
    return cmd;
}

static uint8_t GetCtrlWriteCmd(bool ch, ControllerCommand_t ctrlCmd)
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
        // TODO: replace with init fucntion in source
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
    ExpectReadTempAndReturn(tempExpectedA);
    ExpectAnalogReadAndReturn(adcReadExpectedA);
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
    // initialise the command reg with any old stuff
    uint8_t cmdRegInit = 0U;
    SET_CHANNEL(cmdRegInit, LIFETESTER_CH_A);
    SET_READ_MODE(cmdRegInit);
    SET_RDY_STATUS(cmdRegInit);
    CLEAR_GO_STATUS(cmdRegInit);
    SET_COMMAND(cmdRegInit, DataReg);
    cmdReg = cmdRegInit;
    // Now write to device to request read of cmd reg
    uint8_t readCmdReg = 0U;
    SET_CHANNEL(readCmdReg, LIFETESTER_CH_A);
    SET_READ_MODE(readCmdReg);
    SET_COMMAND(readCmdReg, CmdReg);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(readCmdReg);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // command bits should change to CmdReg request
    uint8_t cmdRegExpected = cmdRegInit;
    SET_COMMAND(cmdRegExpected, CmdReg);
    CHECK_EQUAL(cmdRegExpected, cmdReg);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // now read the data back
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegExpected, cmdReg);
    mock().checkExpectations();
}
// TODO - following test requires implementation of two cmd regs
#if 0
TEST(ControllerTestGroup, GetCmdRegChBOk)
{
    uint8_t cmd = GetCtrlReadCmd(LIFETESTER_CH_B, CmdReg);
    cmdReg = cmd;
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(cmd);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, cmdReg);
    Controller_ConsumeCommand(mockLifeTesterB, mockLifeTesterB);
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmd);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, cmdReg);
    mock().checkExpectations();
}
#endif
TEST(ControllerTestGroup, ResetChannelAOk)
{
    uint8_t resetCmd = 0U;
    SET_CHANNEL(resetCmd, LIFETESTER_CH_A);
    SET_READ_MODE(resetCmd);
    SET_GO_STATUS(resetCmd);
    SET_COMMAND(resetCmd, Reset);
    // write the reset command
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(resetCmd);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(resetCmd, cmdReg);
    mock().expectOneCall("StateMachine_Reset")
        .withParameter("lifeTester", mockLifeTesterA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // Poll the cmd reg - rdy should be set saying cmd done.
    uint8_t readCmdReg = 0U;
    SET_CHANNEL(readCmdReg, LIFETESTER_CH_A);
    SET_READ_MODE(readCmdReg);
    SET_COMMAND(readCmdReg, CmdReg);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(readCmdReg);
    ExpectCommsLedSwitchOff();
    Controller_ReceiveHandler(nBytesSent);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // expect to read cmd reg. Rdy bit set and go bit cleared
    uint8_t cmdRegExpected = 0U;
    SET_CHANNEL(cmdRegExpected, LIFETESTER_CH_A);
    SET_READ_MODE(cmdRegExpected);
    SET_RDY_STATUS(cmdRegExpected);
    CLEAR_GO_STATUS(cmdRegExpected);
    SET_COMMAND(cmdRegExpected, CmdReg);
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegExpected, cmdReg);
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChANotReadyCmdReturnsCmdReg)
{   
    // setup cmdReg in read data reg with go set but ready not set yet.
    cmdReg = 0U;
    SET_CHANNEL(cmdReg, LIFETESTER_CH_A);
    SET_READ_MODE(cmdReg);
    CLEAR_RDY_STATUS(cmdReg);
    SET_GO_STATUS(cmdReg);
    SET_COMMAND(cmdReg, DataReg);
    // Request to read the cmdReg - polling
    uint8_t readCmdReg = 0U;
    SET_CHANNEL(readCmdReg, LIFETESTER_CH_A);
    SET_READ_MODE(readCmdReg);
    SET_COMMAND(readCmdReg, CmdReg);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(readCmdReg);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(CmdReg, GET_COMMAND(cmdReg));
    CHECK(!IS_GO(cmdReg));
    // Master reads from cmd reg - rdy not set. Go is cleared after call
    uint8_t cmdRegExpected = 0U;
    SET_CHANNEL(cmdRegExpected, LIFETESTER_CH_A);
    SET_READ_MODE(cmdRegExpected);
    CLEAR_RDY_STATUS(cmdRegExpected);
    CLEAR_GO_STATUS(cmdRegExpected);
    SET_COMMAND(cmdRegExpected, CmdReg);
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    mock().checkExpectations();
}
#if 0

TEST(ControllerTestGroup, RequestDataFromChBNotReadyCmdReturnsCmdReg)
{   
    uint8_t readDataCmd = GetCtrlReadCmd(LIFETESTER_CH_B, DataReg);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(readDataCmd);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(readDataCmd, cmdReg);
    // Data requested before command consumed - not ready
    uint8_t cmdRegExpected = readDataCmd;
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    CHECK(!bitRead(cmdReg, RDY_BIT));
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegExpected, cmdReg);
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChANotReadyHackDataRdyCmdBits)
{   
    uint8_t readDataCmd = GetCtrlReadCmd(LIFETESTER_CH_A, DataReg);
    // user tries to force data ready status
    uint8_t hackCmd = readDataCmd;
    bitSet(hackCmd, RDY_BIT);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(hackCmd);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // ready bit is reset when command is parsed
    CHECK_EQUAL(readDataCmd, cmdReg);
    CHECK(!bitRead(cmdReg, RDY_BIT));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChAfterReadySet)
{   
    SetExpectedLtDataA(mockLifeTesterA);
    SetExpectedLtDataB(mockLifeTesterB);
    uint8_t readDataReg = GetCtrlReadCmd(LIFETESTER_CH_A, DataReg);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(readDataReg);
    ExpectCommsLedSwitchOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(readDataReg, cmdReg);
    // poll the cmdReg - is the data ready?
    uint8_t cmdRegExpected = readDataReg;
    CHECK(!bitRead(cmdRegExpected, RDY_BIT));
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegExpected, cmdReg);
    // ...no. Command not consumed
    ExpectReadTempAndReturn(tempExpectedA);
    ExpectAnalogReadAndReturn(adcReadExpectedA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // now data is ready according to reg
    CHECK(bitRead(cmdReg, RDY_BIT));
    // master reads it to poll again and check
    cmdRegExpected = readDataReg;
    bitSet(cmdRegExpected, RDY_BIT);
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegExpected, cmdReg);

    #if 0
    uint8_t readCmdReg = GetCtrlReadCmd(LIFETESTER_CH_A, CmdReg);
    ExpectCommsLedSwitchOn();
    ExpectReceiveByte(readCmdReg);
    ExpectCommsLedSwitchOff();
    Controller_ReceiveHandler(nBytesSent);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    uint8_t cmdRegExpected = readCmdReg;
    bitSet(cmdRegExpected, RDY_BIT);
    ExpectCommsLedSwitchOn();
    ExpectTransmitByte(cmdRegExpected);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    // and read out the data
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegExpected, cmdReg);
    CHECK(bitRead(cmdReg, RDY_BIT));
    #endif
    mock().checkExpectations();
}
#endif
