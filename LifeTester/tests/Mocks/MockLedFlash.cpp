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

void Flasher::t(uint32_t onNew, uint32_t offNew)
{
    mock().actualCall("Flasher::t")
        .withParameter("onNew", onNew)
        .withParameter("offNew", offNew);    
}

void Flasher::update(void)
{
    mock().actualCall("Flasher::update");
}

void Flasher::on(void)
{
    mock().actualCall("Flahser::on");
}

void Flasher::off(void)
{
    mock().actualCall("Flasher::off");
}

void Flasher::stopAfter(int16_t n)
{
    mock().actualCall("Flasher::stopAfter")
        .withParameter("n",n);
}

void Flasher::keepFlashing(void)
{
    mock().actualCall("Flasher::keepFlashing");
}