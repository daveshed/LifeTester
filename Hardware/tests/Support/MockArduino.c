#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

#include "Arduino.h"
#include "MockArduino.h"
#include <string.h> //memset

static DigitalPinState_t mockDigitalPins[N_DIGITAL_PINS];

/*******************************************************************************
* PRIVATE FUNCTIONS
*******************************************************************************/
static bool DigitalPinValid(uint8_t pin)
{
    return pin < N_DIGITAL_PINS;   
}

static bool AnalogPinValid(uint8_t pin)
{
    return pin < N_ANALOG_PINS;   
}

/*******************************************************************************
* METHODS FOR GETTING AND MANULLAY SETTING THE STATUS OF HARWARE
*******************************************************************************/
bool IsDigitalPinAssigned(uint8_t pin)
{
    if (!DigitalPinValid(pin))
    {
        FAIL("Digital pin index out of range");        
    }
    return mockDigitalPins[pin].isAssigned;
}

bool IsDigitalPinOutput(uint8_t pin)
{
    if (!DigitalPinValid(pin))
    {
        FAIL("Digital pin index out of range");        
    }
    return mockDigitalPins[pin].isOutput;
}

bool IsDigitalPinHigh(uint8_t pin)
{
    if (!DigitalPinValid(pin))
    {
        FAIL("Digital pin index out of range");        
    }
    return mockDigitalPins[pin].isOn && mockDigitalPins[pin].isAssigned;
}

bool IsDigitalPinLow(uint8_t pin)
{
    if (!DigitalPinValid(pin))
    {
        FAIL("Digital pin index out of range");        
    }
    return !mockDigitalPins[pin].isOn && mockDigitalPins[pin].isAssigned;
}

void ResetDigitalPins(void)
{
    memset(mockDigitalPins, 0U, sizeof(DigitalPinState_t) * N_DIGITAL_PINS);
}

void SetDigitalPinToOutput(uint8_t pin)
{
    if (!DigitalPinValid(pin))
    {
        FAIL("Invalid digital pin argument");        
    }
    mockDigitalPins[pin].isOutput = true;
    mockDigitalPins[pin].isAssigned = true;
}

void SetDigitalPinToInput(uint8_t pin)
{
    if (!DigitalPinValid(pin))
    {
        FAIL("Invalid digital pin argument");        
    }
    mockDigitalPins[pin].isOutput = false;
    mockDigitalPins[pin].isAssigned = true;
}

void SetDigitalPinOutputHigh(uint8_t pin)
{
    if (!IsDigitalPinAssigned(pin))
    {
        FAIL("Cannot turn on pin. Not assigned yet");
    }
    else if (!IsDigitalPinOutput(pin))
    {
        FAIL("Cannot turn on pin. Not set to OUTPUT");
    }
    else
    {
        mockDigitalPins[pin].isOn = true;
    }
}

void SetDigitalPinOutputLow(uint8_t pin)
{
    if (!IsDigitalPinAssigned(pin))
    {
        FAIL("Cannot turn on pin. Not assigned yet");
    }
    else if (!IsDigitalPinOutput(pin))
    {
        FAIL("Cannot turn on pin. Not set to OUTPUT");
    }
    else
    {
        mockDigitalPins[pin].isOn = false;
    }
}


/*******************************************************************************
* MOCK IMPLEMENTATIONS OF ARDUINO FUNCTIONS
*******************************************************************************/
void pinMode(uint8_t pin, uint8_t mode)
{
    mock().actualCall("pinMode")
        .withParameter("pin", pin)
        .withParameter("mode", mode);
    if (mode == INPUT)
    {
        SetDigitalPinToInput(pin);
    }
    else if (mode == OUTPUT)
    {
        SetDigitalPinToOutput(pin);
    }
    else
    {
        FAIL("Invalid mode arg passed to pinMode");
    }
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    mock().actualCall("digitalWrite")
        .withParameter("pin", pin)
        .withParameter("value", value);
    if (value == HIGH)
    {
        SetDigitalPinOutputHigh(pin);
    }
    else if (value == LOW)
    {
        SetDigitalPinOutputLow(pin);
    }
    else
    {
        FAIL("Unkown value arg passed to digitalWrite");
    }
}

int analogRead(uint8_t pin)
{
    mock().actualCall("analogRead")
        .withParameter("pin", pin);
    
    if (!AnalogPinValid(pin))
    {
        FAIL("Invalid pin arg passed to analogRead")
    }
    const int retVal = mock().intReturnValue(); 
    if ((retVal < 0) || (retVal > 1023))
    {
        FAIL("Return value out of range");
    }
    return retVal;
}

long unsigned int millis(void)
{
    mock().actualCall("millis");
    return mock().unsignedLongIntReturnValue();
}