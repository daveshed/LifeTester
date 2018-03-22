#include "StateMachine.h"
#include <stdbool.h>
#include <stdint.h>
#include "Macros.h"

typedef enum TimedEvent_e {
    trackingDelay,
    settleTimeThis,
    samplingThis,
    settleTimeNext,
    samplingNext,
    done,
    nEvents
} TimedEvent_t;

// STATIC TimedEvent_t GetTimedEvent(LifeTester_t *const lifeTester);
STATIC void StateAnalyseTrackingMeasurement(LifeTester_t *const lifeTester);
STATIC void StateAnalyseScanMeasurement(LifeTester_t *const lifeTester);
STATIC void StateError(LifeTester_t *const lifeTester);
STATIC void StateInitialise(LifeTester_t *const lifeTester);
STATIC void StateSampleNextCurrent(LifeTester_t *const lifeTester);
STATIC void StateSampleScanCurrent(LifeTester_t *const lifeTester);
STATIC void StateSampleThisCurrent(LifeTester_t *const lifeTester);
STATIC void StateSetToNextVoltage(LifeTester_t *const lifeTester);
STATIC void StateSetToScanVoltage(LifeTester_t *const lifeTester);
STATIC void StateSetToThisVoltage(LifeTester_t *const lifeTester);
STATIC void StateWaitForTrackingDelay(LifeTester_t *const lifeTester);