// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "Arduino.h"
#include "MockArduino.h"  // mockDigitalPins, mockMillis

// definition of extern variables declared in MockArduino.h
long unsigned int mockMillis;
pinState_t mockDigitalPins[N_DIGITAL_PINS];

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

long unsigned int millis(void)
{
    mock().actualCall("millis");
    return mockMillis;
}