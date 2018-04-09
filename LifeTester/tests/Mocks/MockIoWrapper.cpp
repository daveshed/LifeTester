// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

#include "IoWrapper.h"

// records a copy of the last output set on the dac for each channel.
static uint8_t dacOutput[nChannels];


void DacInit(void)
{
    mock().actualCall("DacInit");
}

void DacSetOutputToActiveVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(*lifeTester->data.vActive, lifeTester->io.dac);
}

void DacSetOutputToThisVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(lifeTester->data.vThis, lifeTester->io.dac);
}

void DacSetOutputToNextVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(lifeTester->data.vNext, lifeTester->io.dac);
}

void DacSetOutputToScanVoltage(LifeTester_t const *const lifeTester)
{
    DacSetOutput(lifeTester->data.vScan, lifeTester->io.dac);
}

void DacSetOutput(uint8_t output, chSelect_t channel)
{
    mock().actualCall("DacSetOutput")
        .withParameter("output", output)
        .withParameter("channel", channel);
    
    // keep a copy of last voltage set on this channel
    dacOutput[channel] = output;
}

uint8_t DacGetOutput(LifeTester_t const *const lifeTester)
{
    const chSelect_t ch = lifeTester->io.dac;
    return dacOutput[ch];
}

bool DacOutputSetToActiveVoltage(LifeTester_t const *const lifeTester)
{
    return (DacGetOutput(lifeTester) == *(lifeTester->data.vActive));
}

bool DacOutputSetToThisVoltage(LifeTester_t const *const lifeTester)
{
    return DacGetOutput(lifeTester) == lifeTester->data.vThis;
}

bool DacOutputSetToNextVoltage(LifeTester_t const *const lifeTester)
{
    return DacGetOutput(lifeTester) == lifeTester->data.vNext;
}

bool DacOutputSetToScanVoltage(LifeTester_t const *const lifeTester)
{
    return DacGetOutput(lifeTester) == lifeTester->data.vScan;
}

void DacSetGain(gainSelect_t requestedGain)
{
    mock().actualCall("DacSetGain")
        .withParameter("requestedGain", requestedGain);
}

gainSelect_t DacGetGain(void)
{
    mock().actualCall("DacGetGain");
    return (gainSelect_t)mock().intReturnValue();
}

void AdcInit(void)
{
    mock().actualCall("AdcInit");
}

uint16_t AdcReadData(const uint8_t channel)
{
    mock().actualCall("AdcReadData")
        .withParameter("channel", channel);
    return mock().unsignedIntReturnValue();
}

uint16_t AdcReadLifeTesterCurrent(LifeTester_t const *const lifeTester)
{
    return AdcReadData(lifeTester->io.adc);
}

bool AdcGetError(void)
{
    mock().actualCall("AdcGetError");
    return mock().boolReturnValue();
}

uint8_t AdcGetGain(const uint8_t channel)
{
    mock().actualCall("AdcGetGain");
    return mock().unsignedIntReturnValue();
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