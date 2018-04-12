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

static uint8_t transmitBuffer[BUFFER_MAX_SIZE];  // length in wire.h 
static LifeTester_t *mockLifeTesterA;
static LifeTester_t *mockLifeTesterB;


/*******************************************************************************
* PRIVATE FUNCTION IMPLEMENTATIONS
*******************************************************************************/
static  uint8_t ReadUint8FromBuffer(uint8_t const *const buffer)
{
    return *buffer;
}

static uint32_t ReadUint32FromBuffer(uint8_t const *const buffer)
{
    uint32_t retVal = 0U;
    for (int i = 0; i < sizeof(uint32_t); i++)
    {
        retVal |= (buffer[i] << (i * 8));
    }
    return retVal;
}

static uint16_t ReadUnit16FromBuffer(uint8_t const *const buffer)
{
    return (uint16_t)(buffer[0U] & (buffer[1U] << 8U));
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

    memcpy(transmitBuffer, data, quantity);
    return (size_t)mock().unsignedIntReturnValue();
}
// instance of TwoWire visible from tests and source via external lilnkage in header
TwoWire Wire = TwoWire();


/*******************************************************************************
 * UNIT TESTS
 ******************************************************************************/
TEST_GROUP(ControllerTestGroup)
{
    void setup(void)
    {
        memset(transmitBuffer, 0U, BUFFER_LENGTH);
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
    ReadUint32FromBuffer(transmitBuffer);
    const uint32_t tExpected = 23432;
    const uint16_t vExpected = 34;
    const uint16_t iExpected = 2345U;
    const ErrorCode_t errorExpected = lowCurrent;
    ActivateThisMeasurement(mockLifeTesterA);

    mockLifeTesterA->timer = tExpected;
    mockLifeTesterA->data.vThis = vExpected;
    mockLifeTesterA->data.iThis = iExpected;
    mockLifeTesterA->error = errorExpected;
    ActivateThisMeasurement(mockLifeTesterB);
    mockLifeTesterB->timer = tExpected;
    mockLifeTesterB->data.vThis = vExpected;
    mockLifeTesterB->data.iThis = iExpected;
    mockLifeTesterB->error = errorExpected;
    mock().expectOneCall("TempGetRawData")
        .andReturnValue(0U);
    mock().expectOneCall("analogRead")
        .withParameter("pin", LIGHT_SENSOR_PIN)
        .andReturnValue(0);  // note returns signed
    WriteDataToBuffer(mockLifeTesterA, mockLifeTesterB);
    mock().expectOneCall("TwoWire::write")
        .withParameter("data", (const void *)buf.d)
        .withParameter("quantity", 17U)
        .andReturnValue(17U);

    Controller_TransmitData();
    PrintBuffer(&buf);
    CHECK_EQUAL(tExpected, ReadUint32FromBuffer(transmitBuffer));
    mock().checkExpectations();
}
