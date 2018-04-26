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

#define RESET_CH_A              (0x3U)
#define RESET_CH_B              (0x83U)
#define READ_CH_A_CMD           (0x0U)
#define READ_CH_B_CMD           (0x80U)
#define WRITE_CH_A_CMD          (0x40U)
#define WRITE_CH_B_CMD          (0xC0U)
#define READ_CH_A_DATA          (0x2U)
#define READ_CH_B_DATA          (0x82U)
#define READ_PARAMS             (0x1U)
#define WRITE_PARAMS            (0x41U)
#define WRITE_CH_B_DATA_BAD_CMD (0xC2U)

static LifeTester_t *mockLifeTesterA;
static LifeTester_t *mockLifeTesterB;
static DataBuffer_t mockRxBuffer;  // data received by device from master see Wire.cpp
// example data used to set mock lifetesters
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
// example measurement params data
const uint16_t settleTime = SETTLE_TIME_MAX / 2U;
const uint16_t trackDelay = TRACK_DELAY_TIME_MAX / 2U;
const uint16_t sampleTime = SAMPLING_TIME_MAX / 2U;
const uint16_t thresholdCurrent = THRESHOLD_CURRENT_MAX / 2U;
/*******************************************************************************
* PRIVATE FUNCTION IMPLEMENTATIONS
*******************************************************************************/
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
    return ReadUint8(&mockRxBuffer);
}

int TwoWire::available(void)
{
    mock().actualCall("TwoWire::available");
    return NumBytes(&mockRxBuffer);    
}

void StateMachine_Reset(LifeTester_t *const lifeTester)
{
    mock().actualCall("StateMachine_Reset")
        .withParameter("lifeTester", lifeTester);
}

// instance of TwoWire visible from tests and source via external lilnkage in header
TwoWire Wire = TwoWire();

/*******************************************************************************
* MOCK FUNCTION EXPECTS FOR TESTS
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
#if 0
static void ExpectTransmitByte(uint8_t byteToSend)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", byteToSend);
}
#endif
static void ExpectSendTransmitBuffer(DataBuffer_t *const buf)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", buf->d)
        .withParameter("quantity", NumBytes(buf));
}

static void ExpectReceiveByte(uint8_t byteReceived)
{
    mock().expectOneCall("TwoWire::read");
    WriteUint8(&mockRxBuffer, byteReceived);
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

static void ExpectReadBufferFlush(void)
{
    mock().expectOneCall("TwoWire::available");
    for (int i = 0; i < NumBytes(&mockRxBuffer); i++)
    {
        mock().expectOneCall("TwoWire::read");
        mock().expectOneCall("TwoWire::available");
    }
}

static void ExpectsForReceiveHandlerRWCmdReg(uint8_t cmd)
{
    ExpectCommsLedSwitchOn();
    ExpectReadBufferFlush();
    ExpectReceiveByte(cmd);
    ExpectCommsLedSwitchOff();
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
        ResetBuffer(&mockRxBuffer);
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
    SET_COMMAND(cmdRegInit, DataReg);
    cmdReg = cmdRegInit;
    // Now write to device to request read of cmd reg
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_A_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // command reg shouldn't change
    CHECK_EQUAL(cmdRegInit, cmdReg);    
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // now read the data back
    ExpectCommsLedSwitchOn();
    WriteUint8(&transmitBuffer, 76);  // try to break test - junk in buffer
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegInit, transmitBuffer.d[0]);
    CHECK_EQUAL(cmdRegInit, cmdReg);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, GetCmdRegChBOk)
{
    // initialise the command reg with any old stuff
    uint8_t cmdRegInit = 0U;
    SET_CHANNEL(cmdRegInit, LIFETESTER_CH_B);
    SET_READ_MODE(cmdRegInit);
    SET_RDY_STATUS(cmdRegInit);
    SET_COMMAND(cmdRegInit, DataReg);
    cmdReg = cmdRegInit;
    // Now write to device to request read of cmd reg
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_B_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // command reg shouldn't change
    CHECK_EQUAL(cmdRegInit, cmdReg);    
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // now read the data back
    ExpectCommsLedSwitchOn();
    WriteUint8(&transmitBuffer, 76);  // try to break test - junk in buffer
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdRegInit, transmitBuffer.d[0]);
    CHECK_EQUAL(cmdRegInit, cmdReg);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, ResetChannelAOk)
{
    // First request a write to the cmd reg
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_A_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK(IS_WRITE(cmdReg))
    CHECK(IS_RDY(cmdReg))
    CHECK_EQUAL(CmdReg, GET_COMMAND(cmdReg));
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // then write the reset command
    ExpectsForReceiveHandlerRWCmdReg(RESET_CH_A);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(Reset, GET_COMMAND(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_A, GET_CHANNEL(cmdReg));
    mock().expectOneCall("StateMachine_Reset")
        .withParameter("lifeTester", mockLifeTesterA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // Poll the cmd reg - rdy should be set saying cmd done.
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_A_CMD);
    Controller_ReceiveHandler(nBytesSent);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // expect to read cmd reg. Rdy bit set and go bit cleared
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(Reset, GET_COMMAND(cmdReg));
    // check that the command reg has been loaded to transmit buffer
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, ResetChannelBOk)
{
    // First request a write to the cmd reg
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_B_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK(IS_WRITE(cmdReg))
    CHECK(IS_RDY(cmdReg))
    CHECK_EQUAL(CmdReg, GET_COMMAND(cmdReg));
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // then write the reset command
    ExpectsForReceiveHandlerRWCmdReg(RESET_CH_B);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(Reset, GET_COMMAND(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_B, GET_CHANNEL(cmdReg));
    mock().expectOneCall("StateMachine_Reset")
        .withParameter("lifeTester", mockLifeTesterB);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // Poll the cmd reg - rdy should be set saying cmd done.
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_B_CMD);
    Controller_ReceiveHandler(nBytesSent);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // expect to read cmd reg. Rdy bit set and go bit cleared
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(Reset, GET_COMMAND(cmdReg));
    // check that the command reg has been loaded to transmit buffer
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChANotReadyRdyBitNotSet)
{   
    // setup cmdReg in read data reg mode with go set but ready not set yet.
    cmdReg = READ_CH_A_DATA;
    // Request to read the cmdReg - polling
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_A_CMD);
    const uint8_t nBytesSent = 1U;
    const uint8_t cmdRegInit = cmdReg;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    // cmd reg should be passed to transmit buffer ready for read
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK_EQUAL(cmdRegInit, cmdReg);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Master reads from cmd reg - rdy not set. Go is cleared after call
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_A, GET_CHANNEL(cmdReg));
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChBNotReadyCmdReturnsCmdReg)
{   
    // setup cmdReg in read data reg mode with go set but ready not set yet.
    cmdReg = READ_CH_B_DATA;
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Request to read the cmdReg - polling
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_B_CMD);
    const uint8_t nBytesSent = 1U;
    const uint8_t cmdRegInit = cmdReg;
    Controller_ReceiveHandler(nBytesSent);
    // cmd reg should be passed to transmit buffer ready for read
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK_EQUAL(cmdRegInit, cmdReg);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Master reads from cmd reg - rdy not set. Go is cleared after call
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_B, GET_CHANNEL(cmdReg));
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChANotReadyHackDataRdyCmdBits)
{   
    uint8_t hackCmd = READ_CH_A_DATA;
    // user tries to force data ready status
    SET_RDY_STATUS(hackCmd);
    ExpectsForReceiveHandlerRWCmdReg(hackCmd);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // ready bit is reset when command is parsed
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // master now reads from device. Expect error raised. Busy/data not ready
    CHECK(IsEmpty(&transmitBuffer));
    ExpectCommsLedSwitchOn();
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(BusyError, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, WritingNewCommandNotReadyRaisesError)
{
    // setup cmd reg with by requesting a write
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_B_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // master then requests a reset on ch b
    ExpectsForReceiveHandlerRWCmdReg(RESET_CH_B);
    Controller_ReceiveHandler(nBytesSent);
    // device is busy. Requesting to write another command will raise error
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_B_CMD);
    Controller_ReceiveHandler(nBytesSent);
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(BusyError, GET_ERROR(cmdReg));
}

TEST(ControllerTestGroup, UnkownCommandRaisesError)
{
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_B_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // write to the data register - not allowed raise error
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_B_DATA_BAD_CMD);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(UnkownCmdError, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, WriteAndReadCmdBackFromCmdReg)
{
    // request to write command for channel A
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_A_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(CmdReg, GET_COMMAND(cmdReg));
    CHECK(IS_WRITE(cmdReg));
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_A, GET_CHANNEL(cmdReg));
    // Now write the command in - request data read ch A
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_A_DATA);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK(!IS_WRITE(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_A, GET_CHANNEL(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChAfterReadyStatusIsSet)
{   
    SetExpectedLtDataA(mockLifeTesterA);
    SetExpectedLtDataB(mockLifeTesterB);
    // read ch data already set
    cmdReg = READ_CH_A_DATA;
    CHECK(!IS_RDY(cmdReg));
    // poll cmd reg to see if data is ready
    const uint8_t nBytesSent = 1U;
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_A_CMD);
    Controller_ReceiveHandler(nBytesSent);
    // Master reads from cmd reg - command not consumed so not ready
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK(!IS_WRITE(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_A, GET_CHANNEL(cmdReg));
    // now command is consumed and data should be loaded to buffer
    ExpectReadTempAndReturn(tempExpectedA);
    ExpectAnalogReadAndReturn(adcReadExpectedA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // now data is ready according to reg
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK(!IS_WRITE(cmdReg));
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_A, GET_CHANNEL(cmdReg));
    CHECK_EQUAL(timeExpectedA, ReadUint32(&transmitBuffer));
    CHECK_EQUAL(vExpectedA, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(iExpectedA, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(tempExpectedA, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(adcReadExpectedA, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(errorExpectedA, ReadUint8(&transmitBuffer));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, RequestDataFromChBfterReadyStatusIsSet)
{   
    SetExpectedLtDataA(mockLifeTesterA);
    SetExpectedLtDataB(mockLifeTesterB);
    // read ch data already set
    cmdReg = READ_CH_B_DATA;
    CHECK(!IS_RDY(cmdReg));
    // poll cmd reg to see if data is ready
    const uint8_t nBytesSent = 1U;
    ExpectsForReceiveHandlerRWCmdReg(READ_CH_B_CMD);
    Controller_ReceiveHandler(nBytesSent);
    // Master reads from cmd reg - command not consumed so not ready
    ExpectCommsLedSwitchOn();
    ExpectSendTransmitBuffer(&transmitBuffer);
    ExpectCommsLedSwitchOff();
    Controller_RequestHandler();
    CHECK_EQUAL(cmdReg, transmitBuffer.d[0]);
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK(!IS_WRITE(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    // now command is consumed and data should be loaded to buffer
    ExpectReadTempAndReturn(tempExpectedB);
    ExpectAnalogReadAndReturn(adcReadExpectedB);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    // now data is ready according to reg
    CHECK_EQUAL(DataReg, GET_COMMAND(cmdReg));
    CHECK(!IS_WRITE(cmdReg));
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    CHECK_EQUAL(LIFETESTER_CH_B, GET_CHANNEL(cmdReg));
    CHECK_EQUAL(timeExpectedB, ReadUint32(&transmitBuffer));
    CHECK_EQUAL(vExpectedB, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(iExpectedB, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(tempExpectedB, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(adcReadExpectedB, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(errorExpectedB, ReadUint8(&transmitBuffer));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, ReadMeasurementParams)
{
    // request to write command reg for channel A
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_A_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // Now write the read params command to reg
    ExpectsForReceiveHandlerRWCmdReg(READ_PARAMS);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(ParamsReg, GET_COMMAND(cmdReg));
    CHECK(!IS_WRITE(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Command is consumed - expect calls getting params to load into buffer
    mock().expectOneCall("Config_GetSettleTime").andReturnValue(settleTime);
    mock().expectOneCall("Config_GetTrackDelay").andReturnValue(trackDelay);
    mock().expectOneCall("Config_GetSampleTime").andReturnValue(sampleTime);
    mock().expectOneCall("Config_GetThresholdCurrent").andReturnValue(thresholdCurrent);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    CHECK_EQUAL((settleTime >> TIMING_BIT_SHIFT),
        ReadUint8(&transmitBuffer));
    CHECK_EQUAL((trackDelay >> TIMING_BIT_SHIFT),
        ReadUint8(&transmitBuffer));
    CHECK_EQUAL((sampleTime >> TIMING_BIT_SHIFT),
        ReadUint8(&transmitBuffer));
    CHECK_EQUAL((thresholdCurrent >> CURRENT_BIT_SHIFT),
        ReadUint8(&transmitBuffer));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, SetMeasurementParams)
{
    // Write some junk into the read buffer - shouldn't break
    WriteUint8(&mockRxBuffer, 32);
    WriteUint8(&mockRxBuffer, 8);
    WriteUint8(&mockRxBuffer, 81);
    // request to write command reg for channel A
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_A_CMD);
    uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // Now write the write params command to reg
    ExpectsForReceiveHandlerRWCmdReg(WRITE_PARAMS);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(ParamsReg, GET_COMMAND(cmdReg));
    CHECK(IS_WRITE(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    // Controller will clear the read buffer ready to read in params
    ExpectReadBufferFlush();
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    CHECK(IS_RDY(cmdReg));
    /*
     Master sends data as 4 byte string of measurement params 
     - expect setters to get called
     */
    ExpectCommsLedSwitchOn();
    const uint8_t settleTimeReduced = settleTime >> TIMING_BIT_SHIFT;
    ExpectReceiveByte(settleTimeReduced);
    mock().expectOneCall("Config_SetSettleTime")
        .withParameter("tSettle", (settleTimeReduced << TIMING_BIT_SHIFT));
    const uint8_t trackDelayReduced = trackDelay >> TIMING_BIT_SHIFT;
    ExpectReceiveByte(trackDelayReduced);
    mock().expectOneCall("Config_SetTrackDelay")
       .withParameter("tDelay", (trackDelayReduced << TIMING_BIT_SHIFT));
    const uint8_t sampleTimeReduced = sampleTime >> TIMING_BIT_SHIFT;
    ExpectReceiveByte(sampleTimeReduced);
    mock().expectOneCall("Config_SetSampleTime")
        .withParameter("tSample", sampleTimeReduced << TIMING_BIT_SHIFT);
    const uint8_t thresholdCurrentReduced = thresholdCurrent >> CURRENT_BIT_SHIFT;
    ExpectReceiveByte(thresholdCurrentReduced);
    mock().expectOneCall("Config_SetThresholdCurrent")
        .withParameter("iThreshold", (thresholdCurrentReduced << CURRENT_BIT_SHIFT));
    ExpectCommsLedSwitchOff();
    // receive handler needs all params in a single transaction
    Controller_ReceiveHandler(PARAMS_REG_SIZE);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    mock().checkExpectations();
}

TEST(ControllerTestGroup, SetMeasurementParamsRegWrongSize)
{    
    ExpectsForReceiveHandlerRWCmdReg(WRITE_CH_A_CMD);
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Now write the write params command to reg
    ExpectsForReceiveHandlerRWCmdReg(WRITE_PARAMS);
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(ParamsReg, GET_COMMAND(cmdReg));
    CHECK(IS_WRITE(cmdReg));
    CHECK(!IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Controller will clear the read buffer ready to read in params
    ExpectReadBufferFlush();
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(Ok, GET_ERROR(cmdReg));
    // Slave only recieves 1 byte not the 4 required for setting params
    ExpectCommsLedSwitchOn();
    // No read expected - device throws data away.
    ExpectReadBufferFlush();
    ExpectCommsLedSwitchOff();
    Controller_ReceiveHandler(nBytesSent);
    // Error raised by controller
    CHECK(IsEmpty(&mockRxBuffer));
    CHECK(IS_RDY(cmdReg));
    CHECK_EQUAL(ParamsReg, GET_COMMAND(cmdReg));
    CHECK(IS_WRITE(cmdReg));
    CHECK_EQUAL(BadParamsError, GET_ERROR(cmdReg));
    mock().checkExpectations();
}