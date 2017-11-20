#include "LifeTesterTypes.h"
/*
function to find MPP by scanning over voltages. Updates v to the MPP. 
also pass in DAC and LifeTester objects.

          STAGE 1             STAGE 2                   STAGE 3
          Set V & wait        Read/Average current      Calculate P
tElapsed  0-------------------settleTime----------------(settleTime + sampleTime) 
*/

//scan voltage and look for max power point
void IV_Scan(LifeTester_t * const lifeTester, const uint16_t startV, const uint16_t finV, const uint16_t dV)
{
  uint32_t v;
  uint32_t vMPP;   //everything needs to be defined as long to calculate power correctly
  uint32_t pMax = 0;
  uint32_t pScan;
  uint32_t iScan = 0;
  uint32_t iMax;
  uint32_t timer = millis();
  uint32_t tElapsed;
  uint16_t nSamples = 0; //number of readings taken during sample time

  Serial.println("IV scan...");
  Serial.println("V, I, P, error, channel");
  
  lifeTester->Led.t(25, 500);
  
  v = startV;
  while(v <= finV)
  {
    tElapsed = millis() - timer;
    lifeTester->Led.update();
    lifeTester->IVData.v = v;
    
    //measurement speed defined by settle time and sample time
    if (tElapsed < SETTLE_TIME)
    {
      //STAGE 1 (DURING SETTLE TIME): SET TO VOLTAGE
      DacSetOutput(v, lifeTester->channel);
      iScan = 0;
    }
      
    else if ((tElapsed >= SAMPLING_TIME) && (tElapsed < (SETTLE_TIME + SAMPLING_TIME)))
    {  
      //STAGE 2: (DURING SAMPLING TIME): KEEP READING ADC AND STORING DATA.
      iScan += AdcReadData(lifeTester->channel);
      nSamples++;
      lifeTester->IVData.iTransmit = iScan / nSamples; //data requested by I2C
    }

    else if (tElapsed >= (SETTLE_TIME + SAMPLING_TIME))
    {
      //STAGE 3: MEASUREMENTS FINISHED. UPDATE MAX POWER IF THERE IS ONE. RESET VARIABLES.
      iScan /= nSamples;
      nSamples = 0;
      pScan = iScan * v;

      //update max power and vMPP if we have found a maximum power point.
      if (pScan > pMax)
      {  
        pMax = iScan * v;
        iMax = iScan;
        vMPP = v;
      }  

      if (iScan >= MAX_CURRENT)
      {
        lifeTester->error = current_limit;  //reached current limit
      }
      if (DacGetErrmsg() != 0) //DAC writing error
      {
        lifeTester->error = DAC_error;
      }
      
      Serial.print(v);
      Serial.print(", ");
      Serial.print(iScan);
      Serial.print(", ");
      Serial.print(pScan);
      Serial.print(", ");
      Serial.print(lifeTester->error);
      Serial.print(", ");
      Serial.println(lifeTester->channel);
      
      timer = millis(); //reset timer
      
      v += dV;  //move to the next point
    }
  }

  Serial.println();
  Serial.print("iMax = ");
  Serial.print(iMax);
  Serial.print(", Vmpp = ");
  Serial.print(vMPP);
  Serial.print(", error = ");
  Serial.println(lifeTester->error, DEC);
  
  if (iMax < I_THRESHOLD)
  {
    lifeTester->error = threshold;  
  }
  
  lifeTester->IVData.v = vMPP;
  //reset DAC
  DacSetOutput(0, lifeTester->channel);
}

/*
 * function to track/update maximum point durint operation
 * Pass in lifetester struct and values are updated
 * only track if we don't have an error and it's time to measure
 * error 3 means that the current was too low to start the measurement (in MPPscan)
 * allow a certain number of error readings before triggering error state/stop measurement
 * start measuring after the tracking delay
 */

void IV_MpptUpdate(LifeTester_t * const lifeTester)
{
  uint32_t tElapsed = millis() - lifeTester->timer;
  
  if ((lifeTester->error != threshold) && (lifeTester->nErrorReads < MAX_ERROR_READS))
  {
    
    if ((tElapsed >= TRACK_DELAY_TIME) && tElapsed < (TRACK_DELAY_TIME + SETTLE_TIME))
    {
      //STAGE 1: SET INITIAL STATE OF DAC V0
      DacSetOutput(lifeTester->IVData.v, lifeTester->channel);
    }
    
    else if ((tElapsed >= (TRACK_DELAY_TIME + SETTLE_TIME)) && (tElapsed < (TRACK_DELAY_TIME + SETTLE_TIME + SAMPLING_TIME)))  
    {
      //STAGE 2: KEEP READING THE CURRENT AND SUMMING IT AFTER THE SETTLE TIME
      lifeTester->IVData.iCurrent += AdcReadData(lifeTester->channel);
      lifeTester->nReadsCurrent++;
    }
    
    else if (tElapsed >= (TRACK_DELAY_TIME + SETTLE_TIME + SAMPLING_TIME) && (tElapsed < (TRACK_DELAY_TIME + 2 * SETTLE_TIME + SAMPLING_TIME)))
    {
      //STAGE 3: STOP SAMPLING. SET DAC TO V1.
      DacSetOutput((lifeTester->IVData.v + DV_MPPT), lifeTester->channel);
    }
    
    else if (tElapsed >= (TRACK_DELAY_TIME + 2*SETTLE_TIME + SAMPLING_TIME) && (tElapsed < (TRACK_DELAY_TIME + 2 * SETTLE_TIME + 2 * SAMPLING_TIME)))
    {
      //STAGE 4: KEEP READING THE CURRENT AND SUMMING IT AFTER ANOTHER SETTLE TIME
      lifeTester->IVData.iNext += AdcReadData(lifeTester->channel);
      lifeTester->nReadsNext++;
    }
    
    else if (tElapsed >= (TRACK_DELAY_TIME + 2 * SETTLE_TIME + 2 * SAMPLING_TIME))
    {
      //STAGE 5: MEASUREMENTS DONE. DO CALCULATIONS
      lifeTester->IVData.iCurrent /= lifeTester->nReadsCurrent; //calculate average
      lifeTester->IVData.pCurrent = lifeTester->IVData.v * lifeTester->IVData.iCurrent; //calculate power now
      lifeTester->nReadsCurrent = 0; //reset counter

      lifeTester->IVData.iNext /= lifeTester->nReadsNext;
      lifeTester->IVData.pNext = (lifeTester->IVData.v + DV_MPPT) * lifeTester->IVData.iNext;
      lifeTester->nReadsNext = 0;

      //if power is lower here, we must be going downhill then move back one point for next loop
      if (lifeTester->IVData.pNext > lifeTester->IVData.pCurrent)
      {
        lifeTester->IVData.v += DV_MPPT;
        lifeTester->Led.stopAfter(2); //two flashes
      }
      else
      {
        lifeTester->IVData.v -= DV_MPPT;
        lifeTester->Led.stopAfter(1); //one flash
      }
            
      //finished measurement now so do error detection
      if (lifeTester->IVData.pCurrent == 0)
      {
        lifeTester->error = low_current;  //low power error
        lifeTester->nErrorReads++;
      }
      else if (lifeTester->IVData.iCurrent >= 4095)
      {
        lifeTester->error = current_limit;  //reached current limit
        lifeTester->nErrorReads++;
      }
      else if (MCP48X2_GetErrmsg()!= 0)
      {
        lifeTester->error = DAC_error;  //DAC writing error
        lifeTester->nErrorReads++;
      }
      else//no error here so reset error counter and err_code to 0
      {
        lifeTester->error = 0;
        lifeTester->nErrorReads = 0;
      }

      Serial.print(lifeTester->IVData.v);
      Serial.print(", ");
      Serial.print(lifeTester->IVData.iCurrent);
      Serial.print(", ");
      Serial.print(lifeTester->IVData.pCurrent);
      Serial.print(", ");
      Serial.print(lifeTester->error,DEC);
      Serial.print(", ");
      Serial.print(analogRead(LIGHT_SENSOR_PIN));
      Serial.print(", ");
      Serial.print(TempReadDegC());
      Serial.print(", ");
      Serial.print(lifeTester->channel);
      Serial.println();

      lifeTester->IVData.iTransmit =
        0.5 * (lifeTester->IVData.iCurrent + lifeTester->IVData.iNext);
      lifeTester->timer = millis(); //reset timer
      lifeTester->IVData.iCurrent = 0;
      lifeTester->IVData.iNext = 0;
    }
  }
  
  else //error condition - trigger LED
  {
    //error condition
    lifeTester->Led.t(500,500);
    lifeTester->Led.keepFlashing();
  }
}
