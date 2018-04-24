// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

#include <stdint.h>
#include "Config.h"

void Config_SetSettleTime(uint16_t tSettle)
{
    mock().actualCall("Config_SetSettleTime")
        .withParameter("tSettle", tSettle);
}

void Config_SetTrackDelay(uint16_t tDelay)
{
    mock().actualCall("Config_SetTrackDelay")
        .withParameter("tDelay", tDelay);
}

void Config_SetSampleTime(uint16_t tSample)
{
    mock().actualCall("Config_SetSampleTime")
        .withParameter("tSample", tSample);
}

void Config_SetThresholdCurrent(uint16_t iThreshold)
{
    mock().actualCall("Config_SetThresholdCurrent")
        .withParameter("iThreshold", iThreshold);
}

uint16_t Config_GetSettleTime(void)
{
    mock().actualCall("Config_GetSettleTime");
    return mock().unsignedIntReturnValue();
}

uint16_t Config_GetTrackDelay(void)
{
    mock().actualCall("Config_GetTrackDelay");
    return mock().unsignedIntReturnValue();
}

uint16_t Config_GetSampleTime(void)
{
    mock().actualCall("Config_GetSampleTime");
    return mock().unsignedIntReturnValue();
}

uint16_t Config_GetThresholdCurrent(void)
{
    mock().actualCall("Config_GetSampleTime");
    return mock().unsignedIntReturnValue();
}
