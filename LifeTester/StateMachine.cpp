#include "Arduino.h"
#include "Config.h"
#include "IoWrapper.h"
#include "LedFlash.h"
#include "LifeTesterTypes.h"
#include "Macros.h"
#include "Print.h"
#include <string.h> // memset
#include "StateMachine.h"
#include "StateMachine_Private.h"

/*******************************************************************************
* PRIVATE STATE DEFINITIONS
*******************************************************************************/
// initialisation
STATIC const LifeTesterState_t StateNone = {
    {
        NULL,   // entry function
        NULL,   // step function
        NULL,   // exit function
        NULL    // transition function
    },          // current state
    NULL,       // parent state pointer
    "StateNone" // label
};

// Parent states that capture more specific states
STATIC const LifeTesterState_t StateScanningMode = {
    {
        ScanningModeEntry, // entry function (print message, led params)
        ScanningModeStep,  // step function (update LED)
        ScanningModeExit,  // exit function
        ScanningModeTran   // transition function
    },                     // current state
    NULL,                  // parent state pointer
    "StateScanningMode"    // label
};

STATIC const LifeTesterState_t StateTrackingMode = {
    {
        TrackingModeEntry, // entry function (print message, led params)
        TrackingModeStep,  // step function (update LED)
        NULL,              // exit function
        TrackingModeTran   // transition function
    },                     // current state
    NULL,                  // parent state pointer
    "StateTrackingMode"    // label
};

// Leaf states
STATIC const LifeTesterState_t StateInitialiseDevice = {
    {
        InitialiseEntry,  // entry function (print message, led params)
        InitialiseStep,   // step function (update LED)
        NULL,             // exit function
        InitialiseTran    // transition function
    },                    // current state
    NULL,                 // parent state pointer
    "StateInitialise"     // label
};

STATIC const LifeTesterState_t StateTrackingDelay = {
    {
        TrackingDelayEntry,       // entry function
        TrackingDelayStep,        // step function
        TrackingDelayExit,        // exit function
        NULL                      // transition function
    },                            // current state
    &StateTrackingMode,           // parent state pointer
    "StateTrackingDelay"          // label
};

STATIC const LifeTesterState_t StateMeasureThisDataPoint = {
    {
        MeasureDataPointEntry,    // entry function
        MeasureDataPointStep,     // step function
        MeasureThisDataPointExit, // exit function
        MeasureDataPointTran      // transition function
    },                            // current state
    &StateTrackingMode,           // parent state pointer
    "StateMeasureThisDataPoint"   // label
};

STATIC const LifeTesterState_t StateMeasureNextDataPoint = {
    {
        MeasureDataPointEntry,    // entry function
        MeasureDataPointStep,     // step function
        MeasureNextDataPointExit, // exit function
        MeasureDataPointTran      // transition function
    },                            // current state
    &StateTrackingMode,           // parent state pointer
    "StateMeasureNextDataPoint"   // label
};

STATIC const LifeTesterState_t StateMeasureScanDataPoint = {
    {
        MeasureDataPointEntry,    // entry function
        MeasureDataPointStep,     // step function
        MeasureScanDataPointExit, // exit function
        MeasureDataPointTran      // transition function
    },                            // current state
    &StateScanningMode,           // parent state pointer
    "StateMeasureScanDataPoint"   // label
};

STATIC const LifeTesterState_t StateError = {
    {
        ErrorEntry,               // entry function
        ErrorStep,                // step function
        NULL,                     // exit function
        NULL                      // transition function
    },                            // current state
    NULL,                         // parent state pointer
    "StateError"                  // label
};
/*******************************************************************************
* HELPER FUNCTIONS
*******************************************************************************/
static void PrintError(ErrorCode_t error)
{
    switch(error)
    {
        case(ok):
            SERIAL_PRINTLN("ok", "%s");
            break;
        case(lowCurrent):
            SERIAL_PRINTLN("error: low current", "%s");
            break;
        case(currentLimit):
            SERIAL_PRINTLN("error: current limit", "%s");
            break;
        case(currentThreshold):
            SERIAL_PRINTLN("error: below current threshold", "%s");
            break;
        case(invalidScan):
            SERIAL_PRINTLN("error: invalid scan shape", "%s");
            break;
        case(DacSetFailed):
            SERIAL_PRINTLN("error: dac set failed", "%s");
            break;
        default:
            SERIAL_PRINTLN("error: unknown", "%s");
            break;
    }
}

static void PrintScanMpp(LifeTester_t const *const lifeTester)
{
    SERIAL_PRINTLNEND();
    SERIAL_PRINT("iMpp = ", "%s");
    SERIAL_PRINT(lifeTester->data.iScanMpp, "%u");
    SERIAL_PRINT(", Vmpp = ", "%s");
    SERIAL_PRINT(lifeTester->data.vScanMpp, "%u");
    SERIAL_PRINT(", ", "%s");
    PrintError(lifeTester->error);
}

static void PrintScanPoint(LifeTester_t const *const lifeTester)
{
    SERIAL_PRINT(lifeTester->io.dac, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(lifeTester->data.vScan, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(lifeTester->data.iScan, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(lifeTester->data.pScan, "%u");
    SERIAL_PRINT(", ", "%s");
    PrintError(lifeTester->error);
}

static void PrintScanHeader(void)
{
    SERIAL_PRINTLN("Scanning for MPP...", "%s");
    SERIAL_PRINTLN("channel, V, I, P, error", "%s");
}

static void PrintNewMpp(LifeTester_t const *const lifeTester)
{
    SERIAL_PRINT(lifeTester->io.dac, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(lifeTester->data.vThis, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(lifeTester->data.iThis, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(lifeTester->data.pThis, "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(analogRead(LIGHT_SENSOR_PIN), "%u");
    SERIAL_PRINT(", ", "%s");
    SERIAL_PRINT(TempReadDegC(), "%f");
    SERIAL_PRINT(", ", "%s");
    PrintError(lifeTester->error);
}

static void PrintMppHeader(void)
{
    SERIAL_PRINTLN("Tracking max power point...", "%s");
    SERIAL_PRINTLN("channel, DACx, ADCx, power, Light Sensor, T(C)", "%s");    
}

static void ResetTimer(LifeTester_t *const lifeTester)
{
    lifeTester->timer = millis(); //reset timer
}

/*
 Prepares lifetester state machine to measure this point by setting pointers 
 to the variables for this point.
*/
STATIC void ActivateThisMeasurement(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    data->vActive = &data->vThis;
    data->iActive = &data->iThis;
    data->pActive = &data->pThis;
}

/*
 Prepares lifetester state machine to measure next point by setting pointers 
 to the variables for this point.
*/
STATIC void ActivateNextMeasurement(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    data->vActive = &data->vNext;
    data->iActive = &data->iNext;
    data->pActive = &data->pNext;
}

/*
 Prepares lifetester state machine to measure next point by setting pointers 
 to the variables for this point.
*/
STATIC void ActivateScanMeasurement(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    data->vActive = &data->vScan;
    data->iActive = &data->iScan;
    data->pActive = &data->pScan;
}

static void ResetForNextMeasurement(LifeTester_t *const lifeTester)
{
    // Reset lifetester data
    lifeTester->data.nSamples = 0U;
    lifeTester->data.iSampleSum = 0U;
    ResetTimer(lifeTester);
}

static void UpdateScanData(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    // Update max power and vMPP if we have found a maximum power point.
    data->pScan = data->iScan * data->vScan;
    if (data->pScan > data->pScanMpp)
    {  
        data->pScanMpp = data->iScan * data->vScan;
        data->iScanMpp = data->iScan;
        data->vScanMpp = data->vScan;
    }  
    // Store first and last powers to check scan shape. 
    if (data->vScan == V_SCAN_MIN)
    {
        data->pScanInitial = data->iScan * data->vScan;
    }
    else if (data->vScan == V_SCAN_MAX)
    {
        data->pScanFinal = data->iScan * data->vScan;
    }
    else
    {
        // do nothing
    }

    PrintScanPoint(lifeTester);
}

/*******************************************************************************
* FUNCTIONS FOR INITIALISE STATE
*******************************************************************************/
STATIC void InitialiseEntry(LifeTester_t *const lifeTester)
{
    // reset all the data and errors
    memset(&lifeTester->data, 0U, sizeof(LifeTesterData_t));
    // initialise timer.
    lifeTester->timer = millis();
    lifeTester->error = ok;
    // Ensure that the dac can be set or else raise an error
    DacSetOutput(0U, lifeTester->io.dac);
    // TODO - move to step fn. Simpler to keep transitions to child step fn only
    if (!(DacGetOutput(lifeTester) == 0U)) 
    {
        DBG_PRINTLN("Dac set error", "%s");
        StateMachineTransitionOnEvent(lifeTester, ErrorEvent);
    }
    // Signal that lifetester is being initialised.
    lifeTester->led.t(INIT_LED_ON_TIME, INIT_LED_OFF_TIME);
    lifeTester->led.keepFlashing();
}

STATIC void InitialiseStep(LifeTester_t *const lifeTester)
{
    lifeTester->led.update();    
    // TODO: Auto gain. Need a new state for this.
    // Check short-circuit current is above required threshold for measurements
    const uint32_t tPresent   = millis();
    const uint32_t tElapsed   = tPresent - lifeTester->timer;
    const bool     stabilised = (tElapsed >= Config_GetSettleTime());

    if (lifeTester->data.nErrorReads > MAX_ERROR_READS)
    {
        // Only transition to error if enough bad readings have happened.
        StateMachineTransitionOnEvent(lifeTester, ErrorEvent);
    }
    else if (!stabilised)
    {
        // don't do anything just keep waiting
    }
    else
    {
        const uint16_t iShortCircuit = AdcReadLifeTesterCurrent(lifeTester);
        SERIAL_PRINT("Initialising. Short-circuit current = ", "%s");
        SERIAL_PRINTLN(iShortCircuit, "%u");
        if (iShortCircuit < Config_GetThresholdCurrent())
        {
            lifeTester->error = currentThreshold;
            lifeTester->data.nErrorReads++;
            DBG_PRINT("Current below threshold error ", "%s");
            DBG_PRINTLN(iShortCircuit, "%u");
        }
        else if (iShortCircuit >= MAX_CURRENT)
        {
            lifeTester->data.nErrorReads++;
            lifeTester->error = currentLimit;
            DBG_PRINT("Current saturated error ", "%s");
            DBG_PRINTLN(iShortCircuit, "%u");
        }
        else
        {
            lifeTester->error = ok;
            StateMachineTransitionOnEvent(lifeTester, MeasurementDoneEvent);
        }
    }
}

STATIC void InitialiseTran(LifeTester_t *const lifeTester,
                           Event_t e)
{
    if (e == MeasurementDoneEvent)
    {
        ActivateScanMeasurement(lifeTester);
        StateMachineTransitionToState(lifeTester, &StateScanningMode);
    }
    else if (e == ErrorEvent)
    {
        StateMachineTransitionToState(lifeTester, &StateError);

    }
    else
    {
        StateMachineTransitionToState(lifeTester, &StateInitialiseDevice);
    }
}

/*******************************************************************************
* FUNCTIONS FOR SCANNING MODE
*******************************************************************************/

STATIC void ScanningModeEntry(LifeTester_t *const lifeTester)
{
    PrintScanHeader();
    lifeTester->led.t(SCAN_LED_ON_TIME, SCAN_LED_OFF_TIME);
    lifeTester->led.keepFlashing();
    lifeTester->data.vScan = 0U;
}

STATIC void ScanningModeTran(LifeTester_t *const lifeTester,
                             Event_t e){

    if (e == MeasurementStartEvent)
    {
        ActivateScanMeasurement(lifeTester);
        StateMachineTransitionToState(lifeTester, &StateMeasureScanDataPoint);
    }
    else if (e == ScanningDoneEvent)  // scanning finished go to tracking.
    {
        ActivateThisMeasurement(lifeTester);
        StateMachineTransitionToState(lifeTester, &StateTrackingMode);
    }
    else if (e == ErrorEvent)
    {
        StateMachineTransitionToState(lifeTester, &StateError);
    }
    else
    {
        /*Don't do anything. Transition function exits and execution returns to
        calling environment (step function)*/
    }
}

STATIC void ScanningModeStep(LifeTester_t *const lifeTester)
{
    lifeTester->led.update();

    LifeTesterData_t *const data = &lifeTester->data;
    if (data->vScan > V_SCAN_MAX)  // scanning done
    {
        // check that the scan is a hill shape
        const bool scanShapeOk = (data->pScanInitial < data->pScanMpp)
                                  && (data->pScanFinal < data->pScanMpp);
        lifeTester->error = (!scanShapeOk) ? invalidScan : lifeTester->error;  
        
        // report max power point
        PrintScanMpp(lifeTester);

        // Update v to max power point if there's no error otherwise set back to initial value.
        // Scanning is done so go to tracking
        if (lifeTester->error == ok)
        {
            data->vThis = data->vScanMpp;
            data->vNext = data->vScanMpp + DV_MPPT;
            StateMachineTransitionOnEvent(lifeTester, ScanningDoneEvent);
        }
        else // error condition so go to error state
        {
            StateMachineTransitionOnEvent(lifeTester, ErrorEvent);
        }
    }
    else
    {
        StateMachineTransitionOnEvent(lifeTester, MeasurementStartEvent);
    }
}

STATIC void ScanningModeExit(LifeTester_t *const lifeTester)
{
    lifeTester->led.off();
}


/*******************************************************************************
* FUNCTIONS FOR SCANNING MEASUREMENT
*******************************************************************************/

STATIC void MeasureScanDataPointExit(LifeTester_t *const lifeTester)
{
    UpdateScanData(lifeTester);
    lifeTester->data.vScan += DV_SCAN;
}

/*******************************************************************************
* FUNCTIONS FOR MEASUREMENT (GENERIC)
********************************************************************************/

STATIC void MeasureDataPointEntry(LifeTester_t *const lifeTester)
{
    /*Transition to error state only if there have been lots of error readings
    in succession*/
    if (lifeTester->data.nErrorReads > MAX_ERROR_READS)
    {
        StateMachineTransitionOnEvent(lifeTester, ErrorEvent);
    }

    // Scan, This or Next is activated in the transition function
    DacSetOutputToActiveVoltage(lifeTester);
    if (!DacOutputSetToActiveVoltage(lifeTester))
    {
        lifeTester->error = DacSetFailed;
        StateMachineTransitionOnEvent(lifeTester, ErrorEvent);
    }
    else
    {
        ResetForNextMeasurement(lifeTester);
    }
}

STATIC void MeasureDataPointStep(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;

    const uint32_t tPresent = millis();
    const uint16_t tSettle = Config_GetSettleTime();
    const uint16_t tSample = Config_GetSampleTime();
    const uint32_t tElapsed = tPresent - lifeTester->timer;
    const bool     readAdc = (tElapsed >= tSettle)
                             && (tElapsed < (tSettle + tSample));
    const bool     samplingExpired = (tElapsed >= (tSettle + tSample));
    const bool     adcRead = (lifeTester->data.nSamples > 0U);

    if (readAdc) // Is it time to read the adc?
    {
        const uint16_t sample = AdcReadLifeTesterCurrent(lifeTester);
        data->iSampleSum += sample;
        data->nSamples++;
    }
    else if (samplingExpired)
    {
        if (adcRead)
        {
            *data->iActive = data->iSampleSum / data->nSamples;
            *data->pActive = *data->vActive * *data->iActive; 
            // Readings are averaged in the transition function for now.
            StateMachineTransitionOnEvent(lifeTester, MeasurementDoneEvent);
        }
        else
        {
            /*Measurement interrupted. Restart timer and try again.
            Note that we'll never leave this state if adc isn't returning data.*/
            lifeTester->timer = tPresent;
        }
    }
    else
    {
        /* Do nothing. Just leave update. More time elapses and then 
        when update is called, the next state will change.*/
    }
}

STATIC void MeasureDataPointTran(LifeTester_t *const lifeTester,
                                     Event_t e)
{
    if (e == MeasurementDoneEvent) 
    {
        // transition child->parent. Exit function will get called.
        StateMachineTransitionToState(lifeTester, lifeTester->state->parent);
    }
    if (e == ErrorEvent)
    {
        StateMachineTransitionToState(lifeTester, &StateError);
    }
    else
    {
        /*Don't do anything. Transition function exits and execution returns to
        calling environment (step function)*/        
    }
}

/*******************************************************************************
* FUNCTIONS FOR TRACKING MODE
********************************************************************************/

STATIC void TrackingModeEntry(LifeTester_t *const lifeTester)
{
    PrintMppHeader();
}

STATIC void TrackingModeStep(LifeTester_t *const lifeTester)
{
    lifeTester->led.update();

    const bool measurementsDone = lifeTester->data.thisDone
                                  && lifeTester->data.nextDone;
    const bool trackDelayDone   = lifeTester->data.delayDone;
    if (lifeTester->data.nErrorReads > MAX_ERROR_READS)
    {
        StateMachineTransitionOnEvent(lifeTester, ErrorEvent);
    }
    else if (!trackDelayDone)
    {
        StateMachineTransitionOnEvent(lifeTester, TrackDelayStartEvent);
    }
    else if (!measurementsDone)
    {
        StateMachineTransitionOnEvent(lifeTester, MeasurementStartEvent);
    }
    else // recalculate working mpp and restart measurements
    {
        UpdateTrackingData(lifeTester);
        lifeTester->data.thisDone = false;
        lifeTester->data.nextDone = false;
        lifeTester->data.delayDone = false;
    }
}

STATIC void TrackingModeTran(LifeTester_t *const lifeTester,
                             Event_t e)
{
    if (e == MeasurementStartEvent)
    {
        if (!lifeTester->data.thisDone)
        {
            ActivateThisMeasurement(lifeTester);
            StateMachineTransitionToState(lifeTester, &StateMeasureThisDataPoint);
        }
        else if (!lifeTester->data.nextDone)
        {
            ActivateNextMeasurement(lifeTester);
            StateMachineTransitionToState(lifeTester, &StateMeasureNextDataPoint);
        }
        else
        {
            // nothing to measure - returns to caller
        }
    }
    else if (e == TrackDelayStartEvent)
    {
        StateMachineTransitionToState(lifeTester, &StateTrackingDelay);
    }
    else if (e == ErrorEvent)
    {
        StateMachineTransitionToState(lifeTester, &StateError);
    }
    else
    {

    }
}

/*******************************************************************************
* FUNCTIONS FOR TRACKING DATA MEASUREMENT
********************************************************************************/

STATIC void MeasureThisDataPointExit(LifeTester_t *const lifeTester)
{
    lifeTester->data.thisDone = true;
    UpdateErrorReadings(lifeTester);
}

STATIC void MeasureNextDataPointExit(LifeTester_t *const lifeTester)
{
    lifeTester->data.nextDone = true;
    UpdateErrorReadings(lifeTester);
}

static void UpdateErrorReadings(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    if (*data->iActive < MIN_CURRENT)
    {
        lifeTester->error = lowCurrent;
        data->nErrorReads++;
    }
    else if (*data->iActive >= MAX_CURRENT)
    {
        lifeTester->error = currentLimit;
        data->nErrorReads++;
    }
    else
    {
        lifeTester->error = ok;
        data->nErrorReads = 0U;
    }
}

/*
 Note: checking that the adc is sampled is not needed. This is treated as a 
 guard and enforced in the transition function - called before this exit
 function.
 ie. This will only be called if guards are satisfied.
*/
static void UpdateTrackingData(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    /*if power is higher at the next point, we must be going uphill so move
    forwards one point for next loop*/
    if (data->pNext > data->pThis)
    {
        data->vThis += DV_MPPT;
        data->vNext = data->vThis + DV_MPPT;
        lifeTester->led.stopAfter(2); //two flashes
    }
    else // otherwise go the other way...
    {
        data->vThis -= DV_MPPT;
        data->vNext = data->vThis + DV_MPPT;
        lifeTester->led.stopAfter(1); //one flash
    }
    PrintNewMpp(lifeTester);
}

STATIC void TrackingDelayEntry(LifeTester_t *const lifeTester)
{
    lifeTester->timer = millis();
}

STATIC void TrackingDelayStep(LifeTester_t *const lifeTester)
{
    const uint32_t tPresent = millis();
    const uint32_t tElapsed = tPresent - lifeTester->timer;
    if (tElapsed >= Config_GetTrackDelay())
    {
        StateMachineTransitionToState(lifeTester, &StateTrackingMode);
    }
}

STATIC void TrackingDelayExit(LifeTester_t *const lifeTester)
{
    lifeTester->data.delayDone = true;
}

/*******************************************************************************
* FUNCTIONS FOR ERROR STATE
********************************************************************************/

STATIC void ErrorEntry(LifeTester_t *const lifeTester)
{
    #ifdef DEBUG
        PrintError(lifeTester->error);
    #endif
    lifeTester->led.t(ERROR_LED_ON_TIME,ERROR_LED_OFF_TIME);
    lifeTester->led.keepFlashing();
    DacSetOutput(0U, lifeTester->io.dac);
}

STATIC void ErrorStep(LifeTester_t *const lifeTester)
{
    lifeTester->led.update();
}

static void ExitCurrentChildState(LifeTester_t *const lifeTester)
{
    StateFn_t *exit = lifeTester->state->fn.exit;
    RUN_STATE_FN(exit, lifeTester);
}

static void ExitCurrentParentState(LifeTester_t *const lifeTester)
{
    if (lifeTester->state->parent != NULL)
    {
        StateFn_t *exit = lifeTester->state->parent->fn.exit;
        RUN_STATE_FN(exit, lifeTester);
    }
}

static void EnterTargetChildState(LifeTester_t *const lifeTester,
                                  LifeTesterState_t const* const targetState)
{
    StateFn_t *entry = targetState->fn.entry;
    RUN_STATE_FN(entry, lifeTester);
}

static void EnterTargetParentState(LifeTester_t *const lifeTester,
                                   LifeTesterState_t const* const targetState)
{
    if (targetState->parent != NULL)
    {
        StateFn_t *entry = targetState->parent->fn.entry;
        RUN_STATE_FN(entry, lifeTester);
    }
}

/*
 Carries out state machine transition by calling the exit function for the 
 current state and entry function for the next.
*/
STATIC void StateMachineTransitionToState(LifeTester_t *const lifeTester,
                                          LifeTesterState_t const *const targetState)
{
    LifeTesterState_t const* state = lifeTester->state;
    DBG_PRINT("Transition from ", "%s");
    DBG_PRINT(state->label, "%s");
    DBG_PRINT("->", "%s");
    DBG_PRINTLN(targetState->label, "%s");

    if (targetState == state)
    {
        // Do nothing. Already there
    }
    else if (targetState == state->parent)
    {
        // only need to exit current state to parent - don't run parent entry
        ExitCurrentChildState(lifeTester);
    }
    else if (targetState->parent == state)
    {
        EnterTargetChildState(lifeTester, targetState);
    }
    else if (targetState->parent == state->parent)
    {
        // Only need to transition out/in one level
        ExitCurrentChildState(lifeTester);
        EnterTargetChildState(lifeTester, targetState);
    }
    else
    {
        // Need to fully exit state and reenter target
        ExitCurrentChildState(lifeTester);
        ExitCurrentParentState(lifeTester);
        EnterTargetParentState(lifeTester, targetState);
        EnterTargetChildState(lifeTester, targetState);
    }
    // Finally transition is done. Copy the target state into lifetester state.
    // memcpy(state, targetState, sizeof(LifeTesterState_t));
    lifeTester->state = targetState;
}

// TODO: what about the transition function of parent?
static void StateMachineTransitionOnEvent(LifeTester_t *const lifeTester,
                                          Event_t e)
{
    StateTranFn_t *TransitionFn = lifeTester->state->fn.tran;
    if (TransitionFn != NULL)
    {
        TransitionFn(lifeTester, e);
    }
}

static void RunChildStepFn(LifeTester_t *const lifeTester)
{
    StateFn_t *step = lifeTester->state->fn.step;
    RUN_STATE_FN(step, lifeTester);
}

static void RunParentStepFn(LifeTester_t *const lifeTester)
{
    LifeTesterState_t const* parent = lifeTester->state->parent;
    if (parent != NULL)
    {
        StateFn_t *step = parent->fn.step;
        RUN_STATE_FN(step, lifeTester);
    }
}

/*******************************************************************************
* PUBLIC API 
*******************************************************************************/
void StateMachine_Reset(LifeTester_t *const lifeTester)
{
    DBG_PRINTLN("Resetting device", "%s");
    lifeTester->state = &StateNone;
    StateMachineTransitionToState(lifeTester, &StateInitialiseDevice);
}

void StateMachine_UpdateStep(LifeTester_t *const lifeTester)
{
    /*Call step functions in this order so that a transition from a NULL parent
    state will only call one step function and one transition. Where as a tran-
    sition from a child state will only call the step fucntion of its parent.
    simpler to debug.*/
    RunParentStepFn(lifeTester);
    RunChildStepFn(lifeTester);
}