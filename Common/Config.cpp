#include <Config.h>

static uint16_t settleTime;
static uint16_t trackDelay;
static uint16_t sampleTime;
static uint16_t thresholdCurrent;

void Config_InitParams(void)
{
    settleTime = SETTLE_TIME;
    trackDelay = TRACK_DELAY_TIME;
    sampleTime = SAMPLING_TIME;
    thresholdCurrent = THRESHOLD_CURRENT;
}

void Config_SetSettleTime(uint16_t tSettle)
{
    settleTime = tSettle;
}

void Config_SetTrackDelay(uint16_t tDelay)
{
    trackDelay = tDelay;
}

void Config_SetSampleTime(uint16_t tSample)
{
    sampleTime = tSample;
}

void Config_SetThresholdCurrent(uint16_t iThreshold)
{
    thresholdCurrent = iThreshold;
}

uint16_t Config_GetSettleTime(void)
{
    return settleTime;
}

uint16_t Config_GetTrackDelay(void)
{
    return trackDelay;
}

uint16_t Config_GetSampleTime(void)
{
    return sampleTime;
}

uint16_t Config_GetThresholdCurrent(void)
{
    return thresholdCurrent;
}