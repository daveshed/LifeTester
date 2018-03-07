#ifndef IV_H
#define IV_H
#include <stdint.h>
#include "LifeTesterTypes.h"

/*
 Function to find MPP by scanning over voltages. Updates v to the MPP. 
 also pass in DAC and LifeTester objects.

           STAGE 1             STAGE 2                   STAGE 3
           Set V & wait        Read/Average current      Calculate P
 tElapsed  0-------------------settleTime----------------(settleTime + sampleTime) 
 measurement speed defined by settle time and sample time
*/
void IV_ScanAndUpdate(LifeTester_t * const lifeTester,
                      const uint16_t startV,
                      const uint16_t finV,
                      const uint16_t dV);

/*
 Function to track/update maximum point durint operation. Pass in lifetester
 struct and values are updated. Only track if we don't have an error and it's
 time to measure allow a certain number of bad readings before triggering error
 state and stopping the measurement. Start measuring after the tracking delay.
 */
void IV_MpptUpdate(LifeTester_t * const lifeTester);

#endif