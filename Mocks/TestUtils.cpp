// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "TestUtils.h"

void CheckMockPinOutputState(int pinNum, bool expectedOn)
{
    // Check that pin has been assigned as output before setting the state
    CHECK_TEXT(mockDigitalPins[pinNum].mode == output,
        "digitalWrite cannot set mode. pinMode not assiged as output.");
    CHECK_TEXT(mockDigitalPins[pinNum].outputOn == expectedOn,
        "pin output state does not match expected state.");
}

void ResetDigitalPins(pinState_t *pins, uint8_t nPins)
{
    // default value for reset
    const pinState_t initValue = {unassigned, false};

    // apply to each pin in turn
    for (int i = 0; i < nPins; i++)
    {
        pins[i] = initValue;
    }
}
