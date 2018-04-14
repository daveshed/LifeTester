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
        .withParameter("data", data)
        .withParameter("quantity", quantity);

    memcpy(&transmitBuffer.d, data, quantity);
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

static void MocksForWireWrite(uint8_t byteTransmitted)
{
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", byteTransmitted);
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
static uint8_t SetCtrlCmd(bool ch, ControllerCommand_t ctrlCmd)
{
    uint8_t cmd = 0U;
    bitInsert(cmd, ctrlCmd, COMMAND_MASK, COMMAND_OFFSET);
    bitWrite(cmd, CH_SELECT_BIT, ch);    
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
    }

    void teardown(void)
    {
        mock().clear();
    }
};

TEST(ControllerTestGroup, DataCopiedToTransmitBufferOk)
{
    const uint32_t timeExpected = 23432;
    const uint16_t vExpected = 34;
    const uint16_t iExpected = 2345U;
    const uint16_t tempExpected = 924U;
    const uint16_t adcReadExpected = 234U;
    const ErrorCode_t errorExpected = lowCurrent;
    mockLifeTesterA->data.vActive = &mockLifeTesterA->data.vThis;
    mockLifeTesterA->data.iActive = &mockLifeTesterA->data.iThis;
    mockLifeTesterA->timer = timeExpected;
    mockLifeTesterA->data.vThis = vExpected;
    mockLifeTesterA->data.iThis = iExpected;
    mockLifeTesterA->error = errorExpected;
 #if 0
    ActivateThisMeasurement(mockLifeTesterB);
    mockLifeTesterB->timer = timeExpected;
    mockLifeTesterB->data.vThis = vExpected;
    mockLifeTesterB->data.iThis = iExpected;
    mockLifeTesterB->error = errorExpected;
#endif
    MocksForReadTempData(tempExpected);
    MocksForAnalogRead(adcReadExpected);
#if 0
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", (const void *)transmitBuffer.d)
        .withParameter("quantity", 13U)
        .andReturnValue(13U);
#endif
    WriteDataToTransmitBuffer(mockLifeTesterA);
    PrintBuffer(&transmitBuffer);
    CHECK_EQUAL(timeExpected, ReadUint32(&transmitBuffer));
    CHECK_EQUAL(vExpected, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(iExpected, ReadUint16(&transmitBuffer));
    // CHECK_EQUAL(vExpected, ReadUint8(&transmitBuffer));
    // CHECK_EQUAL(iExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(tempExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(adcReadExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(lowCurrent, ReadUint8(&transmitBuffer));
    // CHECK_EQUAL(lowCurrent, ReadUint8(&transmitBuffer));
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

TEST(ControllerTestGroup, GetCommsRegChAOk)
{
    uint8_t cmd = SetCtrlCmd(LIFETESTER_CH_A, GetCommsReg);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, commsReg);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    bitSet(cmd, CMD_DONE_BIT);
    MocksForWireWrite(cmd);
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, commsReg);
}

TEST(ControllerTestGroup, GetCommsRegChBOk)
{
    uint8_t cmd = SetCtrlCmd(LIFETESTER_CH_B, GetCommsReg);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, commsReg);
    Controller_ConsumeCommand(mockLifeTesterB, mockLifeTesterB);
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    bitSet(cmd, CMD_DONE_BIT);
    MocksForWireWrite(cmd);
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, commsReg);
}

TEST(ControllerTestGroup, ResetChannelAOk)
{
    uint8_t cmd = SetCtrlCmd(LIFETESTER_CH_A, Reset);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, commsReg);
    mock().expectOneCall("StateMachine_Reset")
        .withParameter("lifeTester", mockLifeTesterA);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    bitSet(cmd, CMD_DONE_BIT);
    MocksForWireWrite(cmd);
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, commsReg);
    // get the command back again
    cmd = SetCtrlCmd(LIFETESTER_CH_A, GetCommsReg);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    Controller_ReceiveHandler(nBytesSent);
    CHECK_EQUAL(cmd, commsReg);
    Controller_ConsumeCommand(mockLifeTesterA, mockLifeTesterB);
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    bitSet(cmd, CMD_DONE_BIT);
    MocksForWireWrite(cmd);
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, commsReg);
}

TEST(ControllerTestGroup, RequestDataFromChANotReady)
{   
    uint8_t cmd = SetCtrlCmd(LIFETESTER_CH_A, GetData);
    MocksForCommsLedOn();
    MocksForWireRead(cmd);
    MocksForCommsLedOff();
    const uint8_t nBytesSent = 1U;
    Controller_ReceiveHandler(nBytesSent);
    // Data requested before command consumed - not ready
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    MocksForWireWrite(cmd);
    Controller_RequestHandler();
    CHECK_EQUAL(cmd, commsReg);
    CHECK(!bitRead(commsReg, DATA_RDY_BIT));
    CHECK(!bitRead(commsReg, CMD_DONE_BIT));
}

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
    CHECK_EQUAL(cmdExpected, commsReg);
    // Data requested before command consumed - not ready
    MocksForCommsLedOn();
    MocksForCommsLedOff();
    MocksForWireWrite(cmdExpected);
    Controller_RequestHandler();
    CHECK_EQUAL(cmdExpected, commsReg);
    CHECK(!bitRead(commsReg, DATA_RDY_BIT));
    CHECK(!bitRead(commsReg, CMD_DONE_BIT));
}
