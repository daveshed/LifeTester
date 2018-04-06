#include "StateMachine.h"
#include "Macros.h"

// Helpers
STATIC void ActivateThisMeasurement(LifeTester_t *const lifeTester);
STATIC void ActivateNextMeasurement(LifeTester_t *const lifeTester);
STATIC void ActivateScanMeasurement(LifeTester_t *const lifeTester);
STATIC void StateMachineTransitionToState(LifeTester_t *const lifeTester,
                                          LifeTesterState_t const *targetState);
static void StateMachineTransitionOnEvent(LifeTester_t *const lifeTester,
                                          Event_t e);
// Entry functions 
STATIC void InitialiseEntry(LifeTester_t *const lifeTester);
STATIC void ScanningModeEntry(LifeTester_t *const lifeTester);
STATIC void TrackingModeEntry(LifeTester_t *const lifeTester);
STATIC void MeasureDataPointEntry(LifeTester_t *const lifeTester);
STATIC void ErrorEntry(LifeTester_t *const lifeTester);

// Step Functions
STATIC void InitialiseStep(LifeTester_t *const lifeTester);
STATIC void MeasureDataPointStep(LifeTester_t *const lifeTester);
STATIC void ScanningModeStep(LifeTester_t *const lifeTester);
STATIC void ErrorStep(LifeTester_t *const lifeTester);

// Exit functions
STATIC void AnalyseTrackingDataExit(LifeTester_t *const lifeTester);
STATIC void MeasureScanDataPointExit(LifeTester_t *const lifeTester);
STATIC void MeasureThisDataPointExit(LifeTester_t *const lifeTester);
// Transition functions
STATIC void InitialiseTran(LifeTester_t *const lifeTester,
                           Event_t e);
STATIC void MeasureThisDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e);
STATIC void MeasureNextDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e);
STATIC void MeasureScanDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e);
STATIC void ScanningModeTran(LifeTester_t *const lifeTester,
                             Event_t e);


#ifdef UNIT_TEST  // give states external linkage for access from tests
extern const LifeTesterState_t StateNone;
extern const LifeTesterState_t StateScanningMode;
extern const LifeTesterState_t StateTrackingMode;
extern const LifeTesterState_t StateInitialiseDevice;
extern const LifeTesterState_t StateMeasureThisDataPoint;
extern const LifeTesterState_t StateMeasureNextDataPoint;
extern const LifeTesterState_t StateMeasureScanDataPoint;
extern const LifeTesterState_t StateError;
#endif