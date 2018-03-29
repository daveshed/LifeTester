#include "StateMachine.h"
#include "Macros.h"

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



STATIC void StateMachineTransitionToState(LifeTester_t *const lifeTester,
                                          LifeTesterState_t targetState);
static void StateMachineTransitionOnEvent(LifeTester_t *const lifeTester,
                                          Event_t e);

STATIC void MeasureThisDataPointEntry(LifeTester_t *const lifeTester);
STATIC void MeasureNextDataPointEntry(LifeTester_t *const lifeTester);
STATIC void MeasureDataPointStep(LifeTester_t *const lifeTester);


// Private state definitions
static const LifeTesterState_t StateNone = {
    .entry = NULL,
    .step = NULL,
    .exit = NULL,
    .idx = NoState,
    "StateNone"
};

static const LifeTesterState_t StateInitialiseDevice = {
    .entry = NULL,
    .step = NULL,
    .exit = NULL,
    .idx = Initialising,
    "StateInitialiseDevice"
};

static const LifeTesterState_t StateMeasureThisDataPoint = {
    .entry = MeasureThisDataPointEntry,
    .step = MeasureDataPointStep,
    .exit = NULL,
    .idx = TrackingThis,
    "StateMeasureThisDataPoint"
};

static const LifeTesterState_t StateMeasureNextDataPoint = {
    .entry = MeasureNextDataPointEntry,
    .step = MeasureDataPointStep,
    .exit = NULL,
    .idx = TrackingNext,
    "StateMeasureNextDataPoint"
};

static const LifeTesterState_t StateMeasureScanDataPoint = {
    .entry = NULL,
    .step = NULL,
    .exit = NULL,
    .idx = Scanning,
    "StateMeasureScanDataPoint"
};

static const LifeTesterState_t StateErrorFoo = {
    .entry = NULL,
    .step = NULL,
    .exit = NULL,
    .idx = Scanning,
    "StateMeasureScanDataPoint"
};

// used for getting the next state from the current state and event
static const LifeTesterState_t StateTransitionTable[MaxNumStates][MaxNumEvents] = 
{
    //NoState -> stay in your current state ie. no transition
    //Transitioning to the same state will rerun entry function 

    // Current State = NoState
    {              // Event
        StateNone, // None
        StateNone, // DacNotSet
        StateNone, // MeasurementDone
        StateNone, // ScanningDone
        StateNone  // ErrorEvent
    },
    // Current State = Initialising
    {              // Event
        StateNone,                  // None
        StateNone,                  // DacNotSet
        StateMeasureScanDataPoint,  // MeasurementDone
        StateNone,                  // ScanningDone
        StateErrorFoo               // ErrorEvent
    },
    // Current State = TrackingThis
    {                               // Event
        StateNone,                  // None
        StateMeasureThisDataPoint,  // DacNotSet - rerun entry function
        StateMeasureNextDataPoint,  // MeasurementDone
        StateNone,                  // ScanningDone
        StateErrorFoo               // ErrorEvent
    },
    // Current State = TrackingNext
    {                               // Event
        StateNone,                  // None
        StateMeasureNextDataPoint,  // DacNotSet - rerun entry function
        StateMeasureThisDataPoint,  // MeasurementDone
        StateNone,                  // ScanningDone
        StateErrorFoo               // ErrorEvent
    },
    // Current State = Scanning
    {                               // Event
        StateNone,                  // None
        StateMeasureScanDataPoint,  // DacNotSet - rerun entry function
        StateMeasureScanDataPoint,  // MeasurementDone
        StateMeasureThisDataPoint,  // ScanningDone
        StateErrorFoo               // ErrorEvent
    },
    // Current State = ErrorState
    {                               // Event
        StateNone,                  // None
        StateNone,                  // DacNotSet
        StateNone,                  // MeasurementDone
        StateNone,                  // ScanningDone
        StateNone                   // ErrorEvent
    }
};