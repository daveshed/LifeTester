/*
 Mock implementation of LedFlash for unit testing only. Class and functions 
 are declared in LedFlash.h
*/
// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

#include "LedFlash.h"

Flasher::Flasher(uint8_t pin)
{
    mock().actualCall("Flasher")
        .withParameter("pin", pin);
}

void Flasher::t(uint32_t onTime, uint32_t offTime)
{
    mock().actualCall("t")
        .withParameter("onTime", onTime)
        .withParameter("pin", offTime);    
}

void Flasher::update(void)
{
    mock().actualCall("update");
}

void Flasher::on(void)
{
    mock().actualCall("on");
}

void Flasher::off(void)
{
    mock().actualCall("off");
}

void Flasher::stopAfter(int16_t n)
{
    mock().actualCall("stopAfter")
        .withParameter("n",n);
}

void Flasher::keepFlashing(void)
{
    mock().actualCall("keepFlashing");
}