// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

#include "IoWrapper.h"

void DacInit(void)
{
    mock().actualCall("DacInit");
}

void DacSetOutput(uint8_t output, chSelect_t channel)
{
    mock().actualCall("DacSetOutput")
        .withParameter("output", output)
        .withParameter("channel", channel);
}

void DacSetGain(gainSelect_t requestedGain)
{
    mock().actualCall("DacSetGain")
        .withParameter("requestedGain", requestedGain);
}

gainSelect_t DacGetGain(void)
{
    mock().actualCall("DacGetGain");
}

void AdcInit(void)
{
    mock().actualCall("AdcInit");
}

uint16_t AdcReadData(const uint8_t channel)
{
    mock().actualCall("AdcReadData")
        .withParameter("channel", channel);
}

bool AdcGetError(void)
{
    mock().actualCall("AdcGetError");
}

uint8_t AdcGetGain(const uint8_t channel)
{
    mock().actualCall("AdcGetGain");
}

void AdcSetGain(const uint8_t gain, const uint8_t channel)
{
    mock().actualCall("AdcSetGain")
        .withParameter("gain", gain)
        .withParameter("channel", channel);
}

void TempSenseInit(void)
{
    mock().actualCall("TempSenseInit");
}

void TempSenseUpdate(void)
{
    mock().actualCall("TempSenseUpdate");
}

uint16_t TempGetRawData(void)
{
    mock().actualCall("TempGetRawData");
}

float TempReadDegC(void)
{
    mock().actualCall("TempReadDegC");
}

bool TempGetError(void)
{
    mock().actualCall("TempGetError");
}