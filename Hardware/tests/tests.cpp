// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

// Code under test
#include "LedFlash.h"
#include "LedFlashPrivate.h"

// support
#include "Arduino.h"
#include <stdbool.h>

/*******************************************************************************
 * Private data and defines
 ******************************************************************************/
#define NUMBER_OF_PINS  (14U)

/*******************************************************************************
 * Private data types used in tests
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

/*******************************************************************************
 * Private test data
 ******************************************************************************/
/*
 Mock digital pin states are held in an array. digital pin 0 is element 0, pin 1
 will be element 1 and so forth. Note that the assignment and output state are
 both held in each element.
 */
static pinState_t mockDigitalPins[NUMBER_OF_PINS];

// value to be returned by millis()
static unsigned long mockMillis;

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
static void resetDigitalPins(pinState_t *pins, uint8_t nPins)
{
    // default value for reset
    const pinState_t initValue = {unassigned, false};

    // apply to each pin in turn
    for (int i = 0; i < nPins; i++)
    {
        pins[i] = initValue;
    }
}

// Creates mock calls for Flahser instantiation
static void mockForFlasherCreateInstance(int pinNum)
{
    mock().expectOneCall("pinMode")
        .withParameter("pin", pinNum).withParameter("mode", OUTPUT);
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
}

static void mockForFlasherUpdateSwitchOff(int pinNum)
{
    mock().expectOneCall("millis");
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
}

static void mockForFlasherUpdateSwitchOn(int pinNum)
{
    mock().expectOneCall("millis");
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", HIGH);
}

static void mockForFlasherUpdateNoSwitch(void)
{
    mock().expectOneCall("millis");
}

// Check that the mock digital pin matches input args
static void checkMockPinOutputState(int pinNum, bool expectedOn)
{
    // Check that pin has been assigned as output before setting the state
    CHECK_TEXT(mockDigitalPins[pinNum].mode == output,
        "digitalWrite cannot set mode. pinMode not assiged as output.");
    CHECK_TEXT(mockDigitalPins[pinNum].outputOn == expectedOn,
        "pin output state does not match expected state.");
}

/*******************************************************************************
 * Mock necessary functions from Arduino.h
 ******************************************************************************/
// Sets the mode of digital pins 2-13 as input/output
void pinMode(uint8_t pin, uint8_t mode)
{
    // check that pin request is in range
    CHECK_TEXT(((pin >= 2U) && (pin <= 13U)),
        "invalid pin number arg in pinMode call.");

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
        FAIL("invalid mode arg passed to pinMode.");
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
    CHECK_TEXT(((pin >= 2U) && (pin <= 13U)),
        "invalid pin number arg in digitalWrite call");

    // Check that pin has been assigned as output before setting the state
    CHECK_TEXT(mockDigitalPins[pin].mode == output,
        "digitalWrite cannot set mode. pinMode not assiged as output.");

    // Now we can write the output state to the selected pin
    if (value == HIGH)
    {
        mockDigitalPins[pin].outputOn = true;
    }
    else if (value == LOW)
    {
        mockDigitalPins[pin].outputOn = false;
    }
    else
    {
        FAIL("Unkown value arg passed to digitalWrite.");
    }

    // Mock low-level Arduino function call
    mock().actualCall("digitalWrite")
        .withParameter("pin", pin).withParameter("value", value);
}

unsigned long millis(void)
{
    mock().actualCall("millis");
    return mockMillis;
}

/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group for LedFlash - all tests share common teardown
TEST_GROUP(LedFlashTestGroup)
{
    void setup(void)
    {
        resetDigitalPins(mockDigitalPins, NUMBER_OF_PINS);

        mockMillis = 0U;
    }

    void teardown(void)
    {
        mock().clear();
    }
};

// Test for instantiating Flasher
TEST(LedFlashTestGroup, Init)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    mock().checkExpectations();

    checkMockPinOutputState(pinNum, false);
}

// Test for turning on the Led constantly
TEST(LedFlashTestGroup, FlasherOnConstant)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mockForFlasherCreateInstance(pinNum);
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
    mockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    // Turn on the Led constantly
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
    testLed.off();

    mock().checkExpectations();
}

// Test for led staying off during off time
TEST(LedFlashTestGroup, FlasherOffStaysOffDuringOffTime)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    // First check that the led is off
    checkMockPinOutputState(pinNum, false);

    /*
     Expect the led to change state at exactly the end of the default off time
     and to remain off in the time up to this.

     NB. mockMillis set to 0 before the test in setup.
    */
    while (mockMillis < DEFAULT_OFF_TIME)
    {
        // Led instance is updated
        mock().expectOneCall("millis");
        testLed.update();

        // check that led is still off
        checkMockPinOutputState(pinNum, false);

        // ...some time passes
        mockMillis += 1U;
    }

    // Finally check the mock function calls match expectations
    mock().checkExpectations();
}

// Test for led turning on at the end of the off period
TEST(LedFlashTestGroup, FlasherTurnOnEndOfOffTime)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    // fast-forward the timer to just before the led should turn on
    mockMillis = DEFAULT_OFF_TIME;
    // Led instance is updated
    mock().expectOneCall("millis");
    testLed.update();
    
    // Now check that the led is still off
    checkMockPinOutputState(pinNum, false);

    mockMillis += 1U; //more time elapses and the led should turn on.

    mockForFlasherUpdateSwitchOn(pinNum);    
    testLed.update();

    // check that led has actually turned on...
    checkMockPinOutputState(pinNum, true);

    // Finally check the mock function calls match expectations
    // mock().checkExpectations();
}

// Test for led staying on during on time
TEST(LedFlashTestGroup, FlasherOnStaysOffDuringOnTime)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    // Fast forward to just after the off time period
    mockMillis = DEFAULT_OFF_TIME + 1U;

    mockForFlasherUpdateSwitchOn(pinNum);
    testLed.update();

    // Check that the led is indeed on at the beginning of period
    checkMockPinOutputState(pinNum, true);

    /*
     Expect the led to change state at exactly the end of the default on time
     and to remain off in the time up to this.

     NB. mockMillis set to 0 before the test in setup.
    */
    while (mockMillis < (DEFAULT_OFF_TIME + DEFAULT_ON_TIME))
    {
        // Led instance is updated
        mock().expectOneCall("millis");
        testLed.update();

        // check that led is still off
        checkMockPinOutputState(pinNum, true);

        // ...some time passes
        mockMillis += 1U;
    }

    // Finally check the mock function calls match expectations
    mock().checkExpectations();
}

/*
 Test for led switching off at the end of the on time. Note that we have to set
 up the test in a long winded way:
 1) create a class instance of flasher - initialised off
 2) go to the end of the off period plus a bit more to ensure led swithces on. At
    this point the time is stored in previousMillis variable
 3) now we need to increment the timer by the on period. After which the led will
    still be on.
 4) incrementing by another ms should then make the led switch off.
 */
TEST(LedFlashTestGroup, FlasherTurnsOffEndOfOnTime)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    mockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    // Fast forward to end of the off period
    mockMillis = DEFAULT_OFF_TIME + 1U;

    // updating led now should turn it on
    mockForFlasherUpdateSwitchOn(pinNum);
    testLed.update();
    // Check that the led is indeed on at the beginning of on period
    checkMockPinOutputState(pinNum, true);

    // fast forward to the end of the on period
    mockMillis += DEFAULT_ON_TIME;

    // led should still be on...
    mockForFlasherUpdateNoSwitch();
    testLed.update();
    // Check that the led is indeed on at the beginning of on period
    checkMockPinOutputState(pinNum, true);

    //...a bit more time passes
    mockMillis += 1U;

    // now the led should switch off
    mockForFlasherUpdateSwitchOff(pinNum);
    testLed.update();
    // check that led has switched off
    checkMockPinOutputState(pinNum, false);

    // Finally check the mock function calls match expectations
    mock().checkExpectations();
}