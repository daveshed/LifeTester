#include "Arduino.h"
#include "Config.h"
#include "IoWrapper.h"
#include "LedFlash.h"
#include "LifeTesterTypes.h"
#include "IV.h"
#include "Macros.h"
#include "Print.h"

//everything needs to be defined as long to calculate power correctly
static uint32_t vMPP;   // maximum power point in dac code   
static uint32_t pMax;   // keeps track of maximum power so far
static uint32_t iSum;   // sum of currents during sampling window
static uint32_t iScan;  // current averaged over sampling window
static uint32_t iMpp;   // current at mpp as adc code
static uint32_t timer;  // ms timer. Decides which measurement stage we're at
static uint16_t nSamples; //number of current readings taken during sample time window
static bool     dacOutputSet = false;

static uint32_t tElapsed;

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

static void PrintMpp(uint32_t iMpp, uint32_t vMPP, errorCode_t error)
{
    DBG_PRINTLN();
    DBG_PRINT("iMpp = ");
    DBG_PRINT(iMpp);
    DBG_PRINT(", Vmpp = ");
    DBG_PRINT(vMPP);
    DBG_PRINT(", error = ");
    PrintError(error);
}

static void PrintScanPoint(uint32_t v,
                           uint32_t iScan,
                           uint32_t pScan,
                           errorCode_t error,
                           chSelect_t channel)
{
    DBG_PRINT(v);
    DBG_PRINT(", ");
    DBG_PRINT(iScan);
    DBG_PRINT(", ");
    DBG_PRINT(pScan);
    DBG_PRINT(", ");
    DBG_PRINT(error);
    DBG_PRINT(", ");
    DBG_PRINTLN(channel);
}

static void PrintScanHeader(void)
{
    DBG_PRINTLN("IV scan...");
    DBG_PRINTLN("V, I, P, error, channel");
}

static void ScanStep(LifeTester_t *const lifeTester, uint32_t *v, uint16_t dV)
{
    const uint32_t voltage = *v;
    tElapsed = millis() - timer;
    lifeTester->led.update();
    lifeTester->data.vScan = voltage;
    //STAGE 1 (DURING SETTLE TIME): SET TO VOLTAGE
    if (tElapsed < SETTLE_TIME)
    {
        DacSetOutputToScanVoltage(lifeTester);
        dacOutputSet = true;
        iSum = 0U;

    }
    //STAGE 2: (DURING SAMPLING TIME): KEEP READING ADC AND STORING DATA.
    else if ((tElapsed >= SETTLE_TIME) && (tElapsed < (SETTLE_TIME + SAMPLING_TIME)))
    {  
        iSum += AdcReadLifeTesterCurrent(lifeTester);
        nSamples++;
        iScan = iSum / nSamples;
    }
    //STAGE 3: MEASUREMENTS FINISHED. UPDATE MAX POWER IF THERE IS ONE. RESET VARIABLES.
    else
    {
        const bool adcRead = (nSamples > 0U); 
        if (dacOutputSet && adcRead)
        {
            // Update max power and vMPP if we have found a maximum power point.
            const uint32_t pScan = iScan * voltage;
            if (pScan > pMax)
            {  
                pMax = iScan * voltage;
                iMpp = iScan;
                vMPP = voltage;
            }  
            // Reached the current limit - flag error which will stop scan
            lifeTester->error = iScan >= MAX_CURRENT ? currentLimit : ok;
            
            PrintScanPoint(voltage, iScan, pScan, lifeTester->error, lifeTester->io.dac);
    
            *v += dV;  //move to the next point only if we've got this one ok.
        }
        // reset timers and flags
        dacOutputSet = false;
        nSamples = 0U;
        timer = millis(); //reset timer      
    }
}  

void IV_ScanAndUpdate(LifeTester_t *const lifeTester,
                      const uint16_t      startV,
                      const uint16_t      finV,
                      const uint16_t      dV)
{
    // Only scan if we're not in an error state
    if (lifeTester->error == ok)
    {
        // copy the initial voltage in case the new mpp can't be found
        const uint32_t initV = lifeTester->data.vThis;
        
        pMax = 0U;
        iSum = 0U;
        timer = millis();
        nSamples = 0U;

        PrintScanHeader();

        // Signal to user that IV is being scanned with led set to fast flash
        lifeTester->led.t(SCAN_LED_ON_TIME, SCAN_LED_OFF_TIME);

        // Run IV scan - record initial and final powers for checking iv scan shape
        uint32_t v = startV;
        uint32_t pInitial;
        uint32_t pFinal;
        
        while((v <= finV) && (lifeTester->error == ok))
        {
            ScanStep(lifeTester, &v, dV);
            // record final and initial power too so we can check scan shape.
            if (v == startV)
            {
                pInitial = iScan * v;
            }
            else if (v == finV)
            {
                pFinal = iScan * v;
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
        PrintMpp(iMpp, vMPP, lifeTester->error);
        // Now set Dac to MPP
        DacSetOutputToThisVoltage(lifeTester);
    }
}

static TimedEvent_t GetTimedEvent(LifeTester_t *const lifeTester)
{
    TimedEvent_t event;
    const uint32_t settleTimeThisStart = TRACK_DELAY_TIME;
    const uint32_t samplingThisStart = settleTimeThisStart + SETTLE_TIME;
    const uint32_t settleTimeNextStart = samplingThisStart + SAMPLING_TIME;
    const uint32_t samplingNextStart = settleTimeNextStart + SETTLE_TIME;
    const uint32_t samplingNextFinish = samplingNextStart + SAMPLING_TIME;

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

static void StateSetToThisVoltage(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == settleTimeThis)
    {
        DacSetOutputToThisVoltage(lifeTester);
        // lifeTester->nextState = StateSetDacToThisVoltage; - breaks tests right now
    }
}

static void StateSampleThisCurrent(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == samplingThis)  
    {
        lifeTester->data.iThis += AdcReadLifeTesterCurrent(lifeTester);
        lifeTester->data.nReadsThis++;
    }
}

static void StateSetToNextVoltage(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == settleTimeNext)
    {
        lifeTester->data.vNext = lifeTester->data.vThis + DV_MPPT;
        DacSetOutputToNextVoltage(lifeTester);
    }    
}

static void StateSampleNextCurrent(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == samplingNext)
    {
        lifeTester->data.iNext += AdcReadLifeTesterCurrent(lifeTester);
        lifeTester->data.nReadsNext++;
    }
}

static void StateAnalyseMeasurement(LifeTester_t *const lifeTester)
{
    if (GetTimedEvent(lifeTester) == done)
    {
    // TODO: readings are summed together and then averaged. Naughty reusing variables
        lifeTester->data.iThis /= lifeTester->data.nReadsThis; //calculate average
        lifeTester->data.pThis = lifeTester->data.vThis * lifeTester->data.iThis; //calculate power now
        lifeTester->data.nReadsThis = 0U; //reset counter

        lifeTester->data.iNext /= lifeTester->data.nReadsNext;
        lifeTester->data.pNext = (lifeTester->data.vNext) * lifeTester->data.iNext;
        lifeTester->data.nReadsNext = 0U;

        //if power is lower here, we must be going downhill then move back one point for next loop
        if (lifeTester->data.pNext > lifeTester->data.pThis)
        {
            lifeTester->data.vThis += DV_MPPT;
            lifeTester->led.stopAfter(2); //two flashes
        }
        else
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
        else//no error here so reset error counter and err_code to 0
        {
            lifeTester->error = ok;
            lifeTester->data.nErrorReads = 0;
        }

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

        lifeTester->timer = millis(); //reset timer
        lifeTester->data.iThis = 0;
        lifeTester->data.iNext = 0;
    }
}

void IV_MpptUpdate(LifeTester_t * const lifeTester)
{
    tElapsed = millis() - lifeTester->timer;
  
    if ((lifeTester->error != currentThreshold) && (lifeTester->data.nErrorReads < MAX_ERROR_READS))
    {
        //STAGE 1: SET INITIAL STATE OF DAC V0
        StateSetToThisVoltage(lifeTester);
        //STAGE 2: KEEP READING THE CURRENT AND SUMMING IT AFTER THE SETTLE TIME
        StateSampleThisCurrent(lifeTester);
        //STAGE 3: STOP SAMPLING. SET DAC TO V1.
        StateSetToNextVoltage(lifeTester);
        //STAGE 4: KEEP READING THE CURRENT AND SUMMING IT AFTER ANOTHER SETTLE TIME
        StateSampleNextCurrent(lifeTester);
        //STAGE 5: MEASUREMENTS DONE. DO CALCULATIONS
        StateAnalyseMeasurement(lifeTester);
    }

    else //error condition - trigger LED
    {
        //error condition
        lifeTester->led.t(500,500);
        lifeTester->led.keepFlashing();
    }
}
