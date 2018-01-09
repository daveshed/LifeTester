// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

// Code under test
#include "LedFlash.h"
#include "LedFlashPrivate.h"

// support
#include "Arduino.h"
#include "MockArduino.h" // mockDigitalPins, mockMillis
#include "TestUtils.h"   // digital pin mocking

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/

// Creates mock calls for Flahser instantiation
static void MockForFlasherCreateInstance(int pinNum)
{
    mock().expectOneCall("pinMode")
        .withParameter("pin", pinNum).withParameter("mode", OUTPUT);
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
}

static void MockForFlasherUpdateSwitchOff(int pinNum)
{
    mock().expectOneCall("millis");
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
}

static void MockForFlasherUpdateSwitchOn(int pinNum)
{
    mock().expectOneCall("millis");
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", HIGH);
}

static void MockForFlasherUpdateNoSwitch(void)
{
    mock().expectOneCall("millis");
}

/*
 runs an Led cycle. Assumes that we're starting at the beginning of the off cycle
 and that the led has already been initialised and is off.
 */
static void RunMockLedFlashCycle(Flasher *led, int pinNum, long onTime, long offTime)
{
    // check that the LED is initialsed and off
    CheckMockPinOutputState(pinNum, false);
    
    // Fast forward to end of the off period
    mockMillis += offTime + 1U;

    // updating led now should turn it on
    MockForFlasherUpdateSwitchOn(pinNum);
    led->update();
    // Check that the led is indeed on at the beginning of on period
    CheckMockPinOutputState(pinNum, true);

    // fast forward to the end of the on period
    mockMillis += onTime;

    // led should still be on...
    MockForFlasherUpdateNoSwitch();
    led->update();
    // Check that the led is indeed on at the beginning of on period
    CheckMockPinOutputState(pinNum, true);

    //...a bit more time passes
    mockMillis += 1U;

    // now the led should switch off
    MockForFlasherUpdateSwitchOff(pinNum);
    led->update();
    // check that led has switched off
    CheckMockPinOutputState(pinNum, false);

    // Finally check the mock function calls match expectations
    mock().checkExpectations();
}

/*******************************************************************************
 * Mock functions
 ******************************************************************************/
/* see MockArduino.c */

/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group for LedFlash - all tests share common setup/teardown
TEST_GROUP(LedFlashTestGroup)
{
    void setup(void)
    {
        ResetDigitalPins(mockDigitalPins, N_DIGITAL_PINS);

        mockMillis = 0U;
    }

    void teardown(void)
    {
        mock().clear();
    }
};

// Test for turning on the Led constantly
TEST(LedFlashTestGroup, FlasherOnConstant)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    MockForFlasherCreateInstance(pinNum);
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
    MockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    // Turn on the Led constantly
    mock().expectOneCall("digitalWrite")
        .withParameter("pin", pinNum).withParameter("value", LOW);
    testLed.off();

    mock().checkExpectations();
}

// Runs a complete cycle of the flashing led using default on/off times.
TEST(LedFlashTestGroup, FlasherCompleteLedFlashCycle)
{
    const int pinNum = 2;

    // Instantiate Flasher class
    MockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);

    RunMockLedFlashCycle(&testLed, pinNum, DEFAULT_ON_TIME, DEFAULT_OFF_TIME);
}

// Tests that we can change the on and off periods from default values
TEST(LedFlashTestGroup, FlasherSettingOnOffTimes)
{
    const int pinNum = 2;
    const long onTime = 50;
    const long offTime = 5000;

    // Instantiate Flasher class
    MockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);
    testLed.t(onTime, offTime);

    RunMockLedFlashCycle(&testLed, pinNum, onTime, offTime);
}

// Test for only two flashes - non-constant operation
TEST(LedFlashTestGroup, FlasherTwoFlashesRequested)
{
    const int pinNum = 2;
    const int numFlashes = 2;

    // Instantiate Flasher class
    MockForFlasherCreateInstance(pinNum);
    Flasher testLed(pinNum);
    testLed.stopAfter(numFlashes);
    // check that the LED is initialsed and off
    CheckMockPinOutputState(pinNum, false);

    for (int i = 0; i < 2; i++)
    {
        RunMockLedFlashCycle(&testLed, pinNum, DEFAULT_ON_TIME, DEFAULT_OFF_TIME);
    }
    /*
     After doing the required number of flashes, the led should be off all the
     time. Updating led now should not do anything - not even call millis. All
     the flashes have been done. Just sit pretty.
     */

    // Fast forward to end of the off period
    mockMillis += DEFAULT_OFF_TIME + 1U;
    
    // Check that the led is still off at the after another off period
    testLed.update();
    CheckMockPinOutputState(pinNum, false);

    // fast forward to the end of the on period
    mockMillis += DEFAULT_ON_TIME;

    // led should still be off
    testLed.update();
    CheckMockPinOutputState(pinNum, false);

    //...a bit more time passes and we're now in the next off period
    mockMillis += 1U;

    // now the led should still be off
    testLed.update();
    CheckMockPinOutputState(pinNum, false);
    // Finally check the mock function calls match expectations
    mock().checkExpectations();
}