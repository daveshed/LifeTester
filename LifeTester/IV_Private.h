#include "IV.h"
#include <stdbool.h>
#include <stdint.h>
#include "Macros.h"

//everything needs to be defined as long to calculate power correctly
static uint32_t vMPP;   // maximum power point in dac code   
static uint32_t pMax;   // keeps track of maximum power so far
static uint32_t iMpp;   // current at mpp as adc code
static bool     dacOutputSet = false;
static uint32_t tElapsed;

typedef enum TimedEvent_e {
    trackingDelay,
    settleTimeThis,
    samplingThis,
    settleTimeNext,
    samplingNext,
    done,
    nEvents
} TimedEvent_t;

STATIC void StateSetToScanVoltage(LifeTester_t *const lifeTester);
STATIC void StateSampleScanCurrent(LifeTester_t *const lifeTester);
STATIC TimedEvent_t GetTimedEvent(LifeTester_t *const lifeTester);
STATIC void StateWaitForTrackingDelay(LifeTester_t *const lifeTester);
STATIC void StateSetToThisVoltage(LifeTester_t *const lifeTester);
STATIC void StateSampleThisCurrent(LifeTester_t *const lifeTester);
STATIC void StateSetToNextVoltage(LifeTester_t *const lifeTester);
STATIC void StateSampleNextCurrent(LifeTester_t *const lifeTester);
STATIC void StateAnalyseMeasurement(LifeTester_t *const lifeTester);