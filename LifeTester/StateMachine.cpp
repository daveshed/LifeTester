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

static void PrintError(errorCode_t error)
{
    switch(error)
    {
        case(ok):
            DBG_PRINTLN("ok");
            break;
        case(lowCurrent):
            DBG_PRINTLN("error: low current error");
            break;
        case(currentLimit):
            DBG_PRINTLN("error: current limit");
            break;
        case(currentThreshold):
            DBG_PRINTLN("error: below current threshold");
            break;
        case(invalidScan):
            DBG_PRINTLN("error: invalid scan shape");
        default:
            DBG_PRINTLN("error: unknown");
            break;
    }
}

static void PrintScanMpp(LifeTester_t const *const lifeTester)
{
    DBG_PRINTLN();
    DBG_PRINT("iMpp = ");
    DBG_PRINT(lifeTester->data.iScanMpp);
    DBG_PRINT(", Vmpp = ");
    DBG_PRINT(lifeTester->data.vScanMpp);
    DBG_PRINT(", error = ");
    PrintError(lifeTester->error);
}

static void PrintScanPoint(LifeTester_t const *const lifeTester)
{
    DBG_PRINT(lifeTester->data.vScan);
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->data.iScan);
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->data.pScan);
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->error);
    DBG_PRINT(", ");
    DBG_PRINTLN(lifeTester->io.dac);
}

static void PrintScanHeader(void)
{
    DBG_PRINTLN("Scanning for MPP...");
    DBG_PRINTLN("V, I, P, error, channel");
}

static void PrintNewMpp(LifeTester_t const *const lifeTester)
{
    DBG_PRINT(lifeTester->data.vThis);
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->data.iThis);
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->data.pThis);
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->error);
    DBG_PRINT(", ");
    DBG_PRINT(analogRead(LIGHT_SENSOR_PIN));
    DBG_PRINT(", ");
    DBG_PRINT(TempReadDegC());
    DBG_PRINT(", ");
    DBG_PRINT(lifeTester->io.dac);
    DBG_PRINTLN();
}

static void PrintMppHeader(void)
{
    DBG_PRINTLN("Tracking max power point...");
    DBG_PRINTLN("DACx, ADCx, power, error, Light Sensor, T(C), channel");    
}

static void ResetTimer(LifeTester_t *const lifeTester)
{
    lifeTester->timer = millis(); //reset timer
}

static void ResetForNextMeasurement(LifeTester_t *const lifeTester)
{
    // Reset lifetester data
    lifeTester->data.nSamples = 0U;
    lifeTester->data.iSampleSum = 0U;
    ResetTimer(lifeTester);
    // Go back to initial state.
    lifeTester->nextState = StateWaitForTrackingDelay;
}

static void UpdateScanData(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    // Update max power and vMPP if we have found a maximum power point.
    data->pScan = data->iScan * data->vScan;
    if (data->pScan > data->pScanMpp)
    {  
        // printf("found a mpp!\n");
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
    // Reached the current limit - flag error which will stop scan
    lifeTester->error =
        (data->iScan >= MAX_CURRENT) ? currentLimit : ok;

    data->vScan += DV_SCAN;  //move to the next point only if we've got this one ok.
}


STATIC TimedEvent_t GetTimedEvent(LifeTester_t *const lifeTester)
{
    TimedEvent_t event;
    const uint32_t settleTimeThisStart = TRACK_DELAY_TIME;
    const uint32_t samplingThisStart = settleTimeThisStart + SETTLE_TIME;
    const uint32_t settleTimeNextStart = samplingThisStart + SAMPLING_TIME;
    const uint32_t samplingNextStart = settleTimeNextStart + SETTLE_TIME;
    const uint32_t samplingNextFinish = samplingNextStart + SAMPLING_TIME;

    const uint32_t tElapsed = millis() - lifeTester->timer;

    if (tElapsed < settleTimeThisStart)
    {
        event = trackingDelay;
    }
    else if ((tElapsed >= settleTimeThisStart) && (tElapsed < samplingThisStart))
    {
        event = settleTimeThis;
    }
    else if ((tElapsed >= samplingThisStart) && (tElapsed < settleTimeNextStart))
    {
        event = samplingThis;
    }
    else if ((tElapsed >= settleTimeNextStart) && (tElapsed < samplingNextStart))
    {
        event = settleTimeNext;
    }
    else if ((tElapsed >= samplingNextStart) && (tElapsed < samplingNextFinish))
    {
        event = samplingNext;
    }
    else
    {
        event = done;
    }
    return event;

}

STATIC void StateInitialise(LifeTester_t *const lifeTester)
{
    // reset all the data and errors
    memset(&lifeTester->data, 0U, sizeof(LifeTesterData_t));
    lifeTester->error = ok;

    // TODO: Auto gain. Need a new state for this.
    // Check short-circuit current is above required threshold for measurements
    DacSetOutput(0U, lifeTester->io.dac);
    delay(POST_DELAY_TIME);
    const uint16_t iShortCircuit = AdcReadLifeTesterCurrent(lifeTester);

    lifeTester->error =
        (iShortCircuit < THRESHOLD_CURRENT) ? currentThreshold : lifeTester->error;
    
    if (lifeTester->error == ok)
    {
        // Setup for a voltage scan
        lifeTester->nextState = StateSetToScanVoltage;
        lifeTester->data.vScan = V_SCAN_MIN;
        lifeTester->timer = millis();
        // Signal to user that IV is being scanned with led set to fast flash
        lifeTester->led.t(SCAN_LED_ON_TIME, SCAN_LED_OFF_TIME);        
        PrintScanHeader();
    }
    else
    {
        lifeTester->nextState = StateError;
    }
}

STATIC void StateWaitForTrackingDelay(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == trackingDelay)
    {
        // Don't do anything. Just wait for time to expire.
    }
    else
    {
        lifeTester->nextState = StateSetToThisVoltage;
    }
}

STATIC void StateSetToScanVoltage(LifeTester_t *const lifeTester)
{
    /* Only set the dac if it hasn't been set already */    
    if (!DacOutputSetToScanVoltage(lifeTester))
    {
        // printf("setting dac voltage to %u\n", lifeTester->data.vScan);
        DacSetOutputToScanVoltage(lifeTester);
        lifeTester->timer = millis();
        // printf("resetting timer to %u\n", lifeTester->timer);
    }

    // transition to next mode and setup data
    lifeTester->data.iScan = 0U;
    lifeTester->data.iSampleSum = 0U;
    lifeTester->data.nSamples = 0U;
    lifeTester->nextState = StateSampleScanCurrent;
}

/*
 State is only responsible for setting the dac if not already set and preparing
 variables for sampling coming next.
*/
STATIC void StateSetToThisVoltage(LifeTester_t *const lifeTester)
{
    if (!DacOutputSetToThisVoltage(lifeTester))
    {
        DacSetOutputToThisVoltage(lifeTester);
    }

    lifeTester->data.iSampleSum = 0U;
    lifeTester->data.nSamples = 0U;
    lifeTester->nextState = StateSampleThisCurrent;
}

STATIC void StateSetToNextVoltage(LifeTester_t *const lifeTester)
{
    if (!DacOutputSetToNextVoltage(lifeTester))
    {
        lifeTester->data.vNext = lifeTester->data.vThis + DV_MPPT;
        DacSetOutputToNextVoltage(lifeTester);
    }

    lifeTester->data.iSampleSum = 0U;
    lifeTester->data.nSamples = 0U;
    lifeTester->nextState = StateSampleNextCurrent;
}

STATIC void StateSampleScanCurrent(LifeTester_t *const lifeTester)
{
    // The dac must be set to the correct voltage otherwise restart the measurement
    if (DacOutputSetToScanVoltage(lifeTester))
    {
        const uint32_t tElapsed = millis() - lifeTester->timer;
        // printf("tElapsed = %u, dac is set...\n", tElapsed);
        
        if ((tElapsed >= SETTLE_TIME)
            && tElapsed < (SAMPLING_TIME + SETTLE_TIME))
        {
            // printf("reading adc...\n");
            lifeTester->data.iSampleSum += AdcReadLifeTesterCurrent(lifeTester);
            lifeTester->data.nSamples++;
            lifeTester->data.iScan = lifeTester->data.iSampleSum / lifeTester->data.nSamples;
        }
        else if (tElapsed >= (SETTLE_TIME + SAMPLING_TIME))
        {
            // printf("going to analyse scan\n");
            lifeTester->nextState = StateAnalyseScanMeasurement;
        }
        else
        {
            lifeTester->nextState = StateSampleScanCurrent;
        }

    }
    else  // dac wasn't set. Restart the measurement
    {
        // printf("adc not set. resetting\n");
        lifeTester->timer = millis();
        lifeTester->nextState = StateSetToScanVoltage;
    }
}

STATIC void StateSampleThisCurrent(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;

    if (DacOutputSetToThisVoltage(lifeTester))
    {
        const TimedEvent_t event = GetTimedEvent(lifeTester);
        
        if (event == samplingThis)
        {
            data->iSampleSum += AdcReadLifeTesterCurrent(lifeTester);
            data->nSamples++;
        }
        else if (event == settleTimeNext)
        {
            if (data->nSamples > 0U)
            {
                lifeTester->nextState = StateSetToNextVoltage;
                // Average current
                data->iThis = data->iSampleSum / data->nSamples;
                // Power
                data->pThis = data->vThis * data->iThis; 
            }
            else
            {
                ResetForNextMeasurement(lifeTester);
            }
        }
        else
        {
            /* Do nothing. Just go leave update. More time elapses and then 
            when update is called, the next state will change.*/
        }

    }
    else  // dac wasn't set. Restart the measurement
    {
        ResetForNextMeasurement(lifeTester);
    }
}

STATIC void StateSampleNextCurrent(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    
    if (DacOutputSetToNextVoltage(lifeTester))
    {
        const TimedEvent_t event = GetTimedEvent(lifeTester);
        
        if (event == samplingNext)
        {
            data->iSampleSum += AdcReadLifeTesterCurrent(lifeTester);
            data->nSamples++;
        }
        else if (event == done)
        {
            if (data->nSamples > 0U)  // done and samples taken. go to next mode.
            {
                // set the mode
                lifeTester->nextState = StateAnalyseTrackingMeasurement;
                // Average the current
                data->iNext = data->iSampleSum / data->nSamples;
                // calculate power
                data->pNext = data->vNext * data->iNext;
            }
            else  // no samples were taken so restart the measurement
            {
                ResetForNextMeasurement(lifeTester); 
            }
        }
        else
        {
            // do nothing. carry on waiting for sampling window.
        }
    }
    else // dac wasn't set. Restart the measurement
    {
        ResetForNextMeasurement(lifeTester);
    }
}

STATIC void StateAnalyseScanMeasurement(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;

    if (data->nSamples > 0U)  // check that the adc has actually been read
    {
        UpdateScanData(lifeTester);   
    }

    /*Check whether the scan has finished or not. If so, check scan shape and 
    set tracking to the correct voltage. If not, then go to the next point.*/
    if (data->vScan <= V_SCAN_MAX)
    {
        // restart measurement or go to next point unless error
        lifeTester->nextState =
            (lifeTester->error == ok) ? StateSetToScanVoltage : StateError;
    } 
    else  // scan finished
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
            lifeTester->nextState = StateSetToThisVoltage;
            PrintMppHeader();
        }
        else // error condition so go to error state
        {
            lifeTester->nextState = StateError;
        }
    }
}

STATIC void StateAnalyseTrackingMeasurement(LifeTester_t *const lifeTester)
{
    if ((GetTimedEvent(lifeTester) == done)
        && (lifeTester->data.nSamples > 0U))  // Readings must have been taken
    {
        /*if power is higher at the next point, we must be going uphill so move
        forwards one point for next loop*/
        if (lifeTester->data.pNext > lifeTester->data.pThis)
        {
            lifeTester->data.vThis += DV_MPPT;
            lifeTester->led.stopAfter(2); //two flashes
        }
        else // otherwise go the other way...
        {
            lifeTester->data.vThis -= DV_MPPT;
            lifeTester->led.stopAfter(1); //one flash
        }
        //finished measurement now so do error detection
        if (lifeTester->data.iThis < MIN_CURRENT)
        {
            lifeTester->error = lowCurrent;  //low current error
            lifeTester->data.nErrorReads++;
        }
        else if (lifeTester->data.iThis >= MAX_CURRENT)
        {
            lifeTester->error = currentLimit;  //reached current limit
            lifeTester->data.nErrorReads++;
        }
        else //no error here so reset error status and counter
        {
            lifeTester->error = ok;
            lifeTester->data.nErrorReads = 0U;
        }

        PrintNewMpp(lifeTester);

    }

    if (lifeTester->error == ok)
    {
        // Begin hill climbing algorithm again
        ResetForNextMeasurement(lifeTester);
    }
    else
    {
        lifeTester->nextState = StateError;
    }
}

STATIC void StateError(LifeTester_t *const lifeTester)
{
    lifeTester->led.t(ERROR_LED_ON_TIME,ERROR_LED_OFF_TIME);
    lifeTester->led.keepFlashing();    
    DacSetOutput(0U, lifeTester->io.dac);
}

/*
 Calls the state function stored in the lifetester object.
*/
static void StateMachineDispatcher(LifeTester_t *const lifeTester)
{
    lifeTester->nextState(lifeTester);
}

void StateMachine_Update(LifeTester_t *const lifeTester)
{
    StateMachineDispatcher(lifeTester);
}

void StateMachine_Initialise(LifeTester_t *const lifeTester)
{
    lifeTester->nextState = StateInitialise;
}