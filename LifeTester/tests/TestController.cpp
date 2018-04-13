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
#include "MockArduino.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
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

int TwoWire::read(void)
{
    mock().actualCall("TwoWire::read");
}

int TwoWire::available(void)
{
    mock().actualCall("TwoWire::read");
    return mock().intReturnValue();
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

/*******************************************************************************
 * UNIT TESTS
 ******************************************************************************/
TEST_GROUP(ControllerTestGroup)
{
    void setup(void)
    {
        ResetBuffer(&transmitBuffer);
        mock().disable();
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
    ActivateThisMeasurement(mockLifeTesterA);
    mockLifeTesterA->timer = timeExpected;
    mockLifeTesterA->data.vThis = vExpected;
    mockLifeTesterA->data.iThis = iExpected;
    mockLifeTesterA->error = errorExpected;
    ActivateThisMeasurement(mockLifeTesterB);
    mockLifeTesterB->timer = timeExpected;
    mockLifeTesterB->data.vThis = vExpected;
    mockLifeTesterB->data.iThis = iExpected;
    mockLifeTesterB->error = errorExpected;
    MocksForReadTempData(tempExpected);
    MocksForAnalogRead(adcReadExpected);
    Controller_WriteDataToBuffer(mockLifeTesterA, mockLifeTesterB);
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", (const void *)transmitBuffer.d)
        .withParameter("quantity", 17U)
        .andReturnValue(17U);
    Controller_TransmitData();
    PrintBuffer(&transmitBuffer);
    CHECK_EQUAL(timeExpected, ReadUint32(&transmitBuffer));
    CHECK_EQUAL(vExpected, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(iExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(vExpected, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(iExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(tempExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(adcReadExpected, ReadUint16(&transmitBuffer));
    CHECK_EQUAL(lowCurrent, ReadUint8(&transmitBuffer));
    CHECK_EQUAL(lowCurrent, ReadUint8(&transmitBuffer));
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