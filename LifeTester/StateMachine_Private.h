#include "StateMachine.h"
#include "Macros.h"

// old state declarations
STATIC void StateAnalyseTrackingMeasurement(LifeTester_t *const lifeTester);
STATIC void StateAnalyseScanMeasurement(LifeTester_t *const lifeTester);
STATIC void StateError(LifeTester_t *const lifeTester);
STATIC void StateSampleNextCurrent(LifeTester_t *const lifeTester);
STATIC void StateSampleScanCurrent(LifeTester_t *const lifeTester);
STATIC void StateSampleThisCurrent(LifeTester_t *const lifeTester);
STATIC void StateSetToNextVoltage(LifeTester_t *const lifeTester);
STATIC void StateSetToScanVoltage(LifeTester_t *const lifeTester);
STATIC void StateSetToThisVoltage(LifeTester_t *const lifeTester);
STATIC void StateWaitForTrackingDelay(LifeTester_t *const lifeTester);

// Helpers
STATIC void ActivateThisMeasurement(LifeTester_t *const lifeTester);
STATIC void ActivateNextMeasurement(LifeTester_t *const lifeTester);
STATIC void ActivateScanMeasurement(LifeTester_t *const lifeTester);
STATIC void StateMachineTransitionToState(LifeTester_t *const lifeTester,
                                          LifeTesterState_t targetState);
static void StateMachineTransitionOnEvent(LifeTester_t *const lifeTester,
                                          Event_t e);
// Entry functions 
STATIC void InitialiseEntry(LifeTester_t *const lifeTester);
STATIC void MeasureDataPointEntry(LifeTester_t *const lifeTester);

// Step Functions
STATIC void InitialiseStep(LifeTester_t *const lifeTester);
STATIC void MeasureDataPointStep(LifeTester_t *const lifeTester);

// Exit functions
STATIC void AnalyseTrackingDataExit(LifeTester_t *const lifeTester);
STATIC void MeasureScanDataPointExit(LifeTester_t *const lifeTester);

// Transition functions
STATIC void InitialiseTran(LifeTester_t *const lifeTester,
                           Event_t e);
STATIC void MeasureThisDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e);
STATIC void MeasureNextDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e);
STATIC void MeasureScanDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e);

// Private state definitions
static const LifeTesterState_t StateNone = {
    .entry = NULL,
    .step = NULL,
    .exit = NULL,
    .tran = NULL,
    .idx = NoState,
    "StateNone"
};

static const LifeTesterState_t StateInitialiseDevice = {
    .entry = InitialiseEntry,
    .step = InitialiseStep,
    .exit = NULL,
    .tran = InitialiseTran,
    .idx = Initialising,
    "StateInitialiseDevice"
};

static const LifeTesterState_t StateMeasureThisDataPoint = {
    .entry = MeasureDataPointEntry,
    .step = MeasureDataPointStep,
    .exit = NULL,
    .tran = MeasureThisDataPointTran,
    .idx = TrackingThis,
    "StateMeasureThisDataPoint"
};

static const LifeTesterState_t StateMeasureNextDataPoint = {
    .entry = MeasureDataPointEntry,
    .step = MeasureDataPointStep,
    .exit = AnalyseTrackingDataExit,
    .tran = MeasureNextDataPointTran,
    .idx = TrackingNext,
    "StateMeasureNextDataPoint"
};

static const LifeTesterState_t StateMeasureScanDataPoint = {
    .entry = MeasureDataPointEntry,
    .step = MeasureDataPointStep,
    .exit = MeasureScanDataPointExit,
    .tran = MeasureScanDataPointTran,
    .idx = Scanning,
    "StateMeasureScanDataPoint"
};

static const LifeTesterState_t StateErrorFoo = {
    .entry = NULL,
    .step = NULL,
    .exit = NULL,
    .tran = NULL,
    .idx = ErrorState,
    "StateErrorFoo"
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