#include "Arduino.h"
#include "Config.h"
#include "IoWrapper.h"
#include "IV_Private.h"
#include "LedFlash.h"
#include "LifeTesterTypes.h"
#include "IV.h"
#include "Macros.h"
#include "Print.h"
#include <string.h> // memset

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

static void PrintScanMpp(uint32_t iMpp, uint32_t vMPP, errorCode_t error)
{
    DBG_PRINTLN();
    DBG_PRINT("iMpp = ");
    DBG_PRINT(iMpp);
    DBG_PRINT(", Vmpp = ");
    DBG_PRINT(vMPP);
    DBG_PRINT(", error = ");
    PrintError(error);
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
    DBG_PRINTLN("IV scan...");
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


static void ResetTimer(LifeTester_t *const lifeTester)
{
    lifeTester->timer = millis(); //reset timer
}

static void ResetForNextMeasurement(LifeTester_t *const lifeTester)
{
    // Reset lifetester data
    lifeTester->data.nReadsThis = 0U;
    lifeTester->data.nReadsNext = 0U;
    lifeTester->data.iThisSum = 0U;
    lifeTester->data.iNextSum = 0U;
    ResetTimer(lifeTester);
    // Go back to initial state.
    lifeTester->nextState = StateWaitForTrackingDelay;
}

STATIC void StateInitialise(LifeTester_t *const lifeTester)
{
    // reset all the data and errors
    memset(&lifeTester->data, 0U, sizeof(LifeTesterData_t));
    lifeTester->error = ok;
    // Setup for a voltage scan
    lifeTester->data.vScan = V_SCAN_MIN;
    lifeTester->timer = millis();
    lifeTester->nextState = StateSetToScanVoltage;
}

STATIC void StateSetToScanVoltage(LifeTester_t *const lifeTester)
{
    /* Only set the dac if it hasn't been set already */    
    if (!DacOutputSetToScanVoltage(lifeTester))
    {
        printf("setting dac voltage to %u\n", lifeTester->data.vScan);
        DacSetOutputToScanVoltage(lifeTester);
        lifeTester->timer = millis();
        printf("resetting timer to %u\n", lifeTester->timer);
    }

    // transition to next mode and setup data
    lifeTester->data.iScan = 0U;
    lifeTester->data.iScanSum = 0U;
    lifeTester->data.nReadsScan = 0U;
    lifeTester->nextState = StateSampleScanCurrent;
}

STATIC void StateSampleScanCurrent(LifeTester_t *const lifeTester)
{
    // The dac must be set to the correct voltage otherwise restart the measurement
    if (DacOutputSetToScanVoltage(lifeTester))
    {
        const uint32_t tElapsed = millis() - lifeTester->timer;
        printf("tElapsed = %u, dac is set...\n", tElapsed);
        
        if ((tElapsed >= SETTLE_TIME)
            && tElapsed < (SAMPLING_TIME + SETTLE_TIME))
        {
            printf("reading adc...\n");
            lifeTester->data.iScanSum += AdcReadLifeTesterCurrent(lifeTester);
            lifeTester->data.nReadsScan++;
            lifeTester->data.iScan = lifeTester->data.iScanSum / lifeTester->data.nReadsScan;
        }
        else if (tElapsed >= (SETTLE_TIME + SAMPLING_TIME))
        {
            printf("going to analyse scan\n");
            lifeTester->nextState = StateAnalyseScanMeasurement;
        }
        else
        {
            lifeTester->nextState = StateSampleScanCurrent;
        }

    }
    else  // dac wasn't set. Restart the measurement
    {
        printf("adc not set. resetting\n");
        lifeTester->timer = millis();
        lifeTester->nextState = StateSetToScanVoltage;
    }
}

STATIC void StateAnalyseScanMeasurement(LifeTester_t *const lifeTester)
{
    LifeTesterData_t *const data = &lifeTester->data;
    const bool adcRead = (data->nReadsScan > 0U); 

    if (adcRead)  // check that the adc has actually been read
    {
        printf("adc read.\n");
        // Update max power and vMPP if we have found a maximum power point.
        data->pScan = data->iScan * data->vScan;
        if (data->pScan > data->pScanMpp)
        {  
            printf("found a mpp!\n");
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

    /*Check whether the scan has finished or not. If so, check scan shape and 
    set tracking to the correct voltage. If not, then go to the next point.*/
    if (data->vScan <= V_SCAN_MAX)
    {
        // restart measurement or go to next point
        lifeTester->nextState = StateSetToScanVoltage;
    } 
    else
    {

        // check that the scan is a hill shape
        const bool scanShapeOk = (data->pScanInitial < data->pScanMpp)
                                 && (data->pScanFinal < data->pScanMpp);
        lifeTester->error = (!scanShapeOk) ? invalidScan : lifeTester->error;  
        
        // Check max is above required threshold for measurement to start
        lifeTester->error =
            (data->iScanMpp < THRESHOLD_CURRENT) ? currentThreshold : lifeTester->error;  

        // report max power point
        PrintScanMpp(data->iScanMpp, data->vScanMpp, lifeTester->error);

        // Update v to max power point if there's no error otherwise set back to initial value.
        // Scanning is done so go to tracking
        if (lifeTester->error == ok)
        {
            data->vThis = data->vScanMpp;
            lifeTester->nextState = StateSetToThisVoltage;
        }
        else
        {
            // TODO: go to error state
        }
    }
}


/*
 Calls the state function stored in the lifetester object.
*/
static void StateMachineDispatcher(LifeTester_t *const lifeTester)
{
    lifeTester->nextState(lifeTester);
}

void IV_ScanUpdate(LifeTester_t *const lifeTester)
{
    StateMachineDispatcher(lifeTester);
}

static void ScanStep(LifeTester_t *const lifeTester, uint16_t dV)
{
    StateMachineDispatcher(lifeTester);
}  
#if 0
void IV_ScanAndUpdate(LifeTester_t *const lifeTester,
                      const uint16_t      startV,
                      const uint16_t      finV,
                      const uint16_t      dV)
{
    lifeTester->nextState = StateSetToScanVoltage;
    // Only scan if we're not in an error state
    if (lifeTester->error == ok)
    {
        // copy the initial voltage in case the new mpp can't be found
        const uint32_t initV = lifeTester->data.vThis;
        
        pMax = 0U;
        lifeTester->data.iScanSum = 0U;
        lifeTester->timer = millis();
        lifeTester->data.nReadsScan = 0U;

        PrintScanHeader();

        // Signal to user that IV is being scanned with led set to fast flash
        lifeTester->led.t(SCAN_LED_ON_TIME, SCAN_LED_OFF_TIME);

        // Run IV scan - record initial and final powers for checking iv scan shape
        lifeTester->data.vScan = startV;
        uint32_t pInitial;
        uint32_t pFinal;
        
        while((lifeTester->data.vScan <= finV) && (lifeTester->error == ok))
        {
            ScanStep(lifeTester, dV);
            // record final and initial power too so we can check scan shape.
            if (lifeTester->data.vScan == startV)
            {
                pInitial = lifeTester->data.iScan * lifeTester->data.vScan;
            }
            else if (lifeTester->data.vScan == finV)
            {
                pFinal = lifeTester->data.iScan * lifeTester->data.vScan;
            }
            else
            {
                // do nothing
            }
        }

        // check that the scan is a hill shape
        const bool scanShapeOk = (pInitial < pMax) && (pFinal < pMax);
        lifeTester->error = (!scanShapeOk) ? invalidScan : lifeTester->error;  
        
        // Check max is above required threshold for measurement to start
        lifeTester->error =
            (iMpp < THRESHOLD_CURRENT) ? currentThreshold : lifeTester->error;  

        // Update v to max power point if there's no error otherwise set back to initial value.
        lifeTester->data.vThis =
            (lifeTester->error == ok) ? vMPP : initV;
        // report max power point
        PrintScanMpp(iMpp, vMPP, lifeTester->error);
        // Now set Dac to MPP
        DacSetOutputToThisVoltage(lifeTester);
    }
}
#endif
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

STATIC void StateSetToThisVoltage(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == settleTimeThis)
    {
        DacSetOutputToThisVoltage(lifeTester);
    }
    else
    {
        lifeTester->nextState = StateSampleThisCurrent;
    }
}

STATIC void StateSampleThisCurrent(LifeTester_t *const lifeTester)
{
    if (DacOutputSetToThisVoltage(lifeTester))
    {
        if (GetTimedEvent(lifeTester) == samplingThis)
        {
            lifeTester->data.iThisSum += AdcReadLifeTesterCurrent(lifeTester);
            lifeTester->data.nReadsThis++;
        }
        else
        {
            lifeTester->nextState = StateSetToNextVoltage;
        }

    }
    else  // dac wasn't set. Restart the measurement
    {
        ResetForNextMeasurement(lifeTester);
    }
}

STATIC void StateSetToNextVoltage(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == settleTimeNext)
    {
        lifeTester->data.vNext = lifeTester->data.vThis + DV_MPPT;
        DacSetOutputToNextVoltage(lifeTester);
    }
    else
    {
        lifeTester->nextState = StateSampleNextCurrent;
    }    
}

STATIC void StateSampleNextCurrent(LifeTester_t *const lifeTester)
{
    if (DacOutputSetToNextVoltage(lifeTester))
    {
        if (GetTimedEvent(lifeTester) == samplingNext)
        {
            lifeTester->data.iNextSum += AdcReadLifeTesterCurrent(lifeTester);
            lifeTester->data.nReadsNext++;
        }
        else
        {
            lifeTester->nextState = StateAnalyseMeasurement;
        }
    }
    else // dac wasn't set. Restart the measurement
    {
        ResetForNextMeasurement(lifeTester);
    }
}

STATIC void StateAnalyseMeasurement(LifeTester_t *const lifeTester)
{
    if ((GetTimedEvent(lifeTester) == done)
        && (lifeTester->data.nReadsThis > 0U)  // Readings must have been taken
        && (lifeTester->data.nReadsNext > 0U))
    {
        // Average current
        lifeTester->data.iThis =
            lifeTester->data.iThisSum + lifeTester->data.nReadsThis;
        // Power
        lifeTester->data.pThis = lifeTester->data.vThis * lifeTester->data.iThis; 

        // Similarly for the next point at +DV_MPPT
        lifeTester->data.iNext =
            lifeTester->data.iNextSum + lifeTester->data.nReadsNext;
        lifeTester->data.pNext = lifeTester->data.vNext * lifeTester->data.iNext;
        
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
            lifeTester->error = lowCurrent;  //low power error
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
    // Begin hill climibing algorigthm again
    ResetForNextMeasurement(lifeTester);
}

void IV_MpptUpdate(LifeTester_t * const lifeTester)
{
    if ((lifeTester->error != currentThreshold) && (lifeTester->data.nErrorReads < MAX_ERROR_READS))
    {
        StateMachineDispatcher(lifeTester);
    }

    else //error condition - trigger LED
    {
        lifeTester->led.t(500,500);
        lifeTester->led.keepFlashing();
    }
}
