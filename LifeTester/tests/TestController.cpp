// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "Controller.h"

// support
#include "Wire.h"
#include "MockArduino.h"

// intantiates the wire interface object
TwoWire::TwoWire()
{
}

// Mocks needed for reading data in update function
size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    mock().actualCall("Wire::write")
        .withParameter("data", data)
        .withParameter("quantity", quantity);    
    return (size_t)mock().unsignedIntReturnValue();
}

TwoWire Wire = TwoWire();
/*******************************************************************************
 * UNIT TESTS
 ******************************************************************************/
TEST_GROUP(I2cTestGroup)
{
    void setup(void)
    {
        // nothing to do yet
    }

    void teardown(void)
    {
        mock().clear();
    }
};

// Dummy test to check things are working
TEST(I2cTestGroup, dummy)
{
    FAIL("Fail me!");
    // check function calls
    mock().checkExpectations();
}
