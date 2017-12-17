// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

// Code under test
#include "LedFlash.h"

// support
#include "Arduino.h"
#include <stdbool.h>
/*******************************************************************************
 * Mock necessary functions from Arduino.h
 ******************************************************************************/
// types to hold information about mock digital pins.
typedef enum pinAssignment_e{
    input,
    output,
    unassigned
} pinAssignment_t;

typedef struct pinState_s{
    pinAssignment_t mode;
    bool            outputOn;
} pinState_t;

/*
 Mock digital pin states are held in an array. digital pin 0 is element 0, pin 1
 will be element 1 and so forth. Note that the assignment and output state are
 both held in each element.
 */
static pinState_t mockDigitalPins[14] = {
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
    {unassigned, false},
};

// Sets the mode of digital pins 2-13 as input/output
void pinMode(uint8_t pin, uint8_t mode)
{
    // check that pin request is in range
    CHECK(pin >= 2U);
    CHECK(pin <= 13U);

    // check valid mode request
    CHECK((mode == INPUT) || (mode == OUTPUT));

    // update mock digital pins
    if (mode == INPUT)
    {
        mockDigitalPins[pin].mode = input;
    }
    else if (mode == OUTPUT)
    {
        mockDigitalPins[pin].mode = output;
    }
    else
    {
        // Defensive. Shouldn't need this because of CHECKS above
        mockDigitalPins[pin].mode = unassigned;
    }

    // mock function behaviour
    mock().actualCall("pinMode")
        .withParameter("pin", pin).withParameter("mode", mode);
}

/*
 sets the state of digital pins 2-13. Note that they must be asigned as output
 first with pinMode.
*/
void digitalWrite(uint8_t pin, uint8_t value)
{
    // check that pin request is in range
    CHECK(pin >= 2U);
    CHECK(pin <= 13U);

    // Check that pin has been assigned as output before setting the state
    CHECK(mockDigitalPins[pin].mode == output);

    // Now we can write the output state to the selected pin
    mockDigitalPins[pin].outputOn = true;

    // Mock low-level Arduino function call
    mock().actualCall("digitalWrite")
        .withParameter("pin", pin).withParameter("value", value);
}

unsigned long millis(void)
{
    mock().actualCall("millis");
    return 1UL;
}

/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group for LedFlash - all tests share common teardown
TEST_GROUP(LedFlashTestGroup)
{
    void teardown()
    {
        mock().clear();
    }
};

// Test for instantiating Flasher
TEST(LedFlashTestGroup, Init)
{
   const int pinNum = 2;
   
   mock().expectOneCall("pinMode")
    .withParameter("pin", pinNum).withParameter("mode", OUTPUT);
   Flasher testLed(pinNum);
   
   mock().checkExpectations();
}

// Test for turning on the Led constantly
TEST(LedFlashTestGroup, FlasherOnConstant)
{
    const int pinNum = 2;
    // Instantiate Flasher class
    mock().expectOneCall("pinMode")
        .withParameter("pin", pinNum).withParameter("mode", OUTPUT);
    Flasher testLed(pinNum);

    // Turn on the Led constantly
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", HIGH);
    testLed.on();
   
    mock().checkExpectations();
}

// Test for turning off the Led constantly
TEST(LedFlashTestGroup, FlasherOffConstant)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mock().expectOneCall("pinMode")
        .withParameter("pin", pinNum).withParameter("mode", OUTPUT);
    Flasher testLed(pinNum);

    // Turn on the Led constantly
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
    testLed.off();

    mock().checkExpectations();
}