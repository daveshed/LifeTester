// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "IV.h"
#include "IV_Private.h"

// support
#include "Arduino.h"   // arduino function prototypes eg. millis (defined here)
#include "Config.h"
#include "IoWrapper.h"
#include <stdio.h>     // REMOVE ME!
#include <string.h>

/*******************************************************************************
 * Private data and defines used for tests
 ******************************************************************************/
#define CURRENT_TO_CODE  (0.8)      // arbitrary scaling used to calculate adc code from current
#define FIXED_CURRENT    (2459U)    // fixed current output from bad device

enum lookupColumn {voltageData, currentData, powerData};

/*
 handles interrupting iv scan
*/
typedef struct MockInterrupt_s {
    bool     requested; // do you want to interrupt?
    uint32_t start;     // start of interrupt
    uint32_t duration;  // duration of interrupt
} MockInterrupt_t;

/*
 Type holds information regarding test timers.
*/
typedef struct TestTiming_s {
    uint32_t        initial;  // initial time at beginning of call
    uint32_t        mock;     // mock time returned from millis
    uint32_t        elapsed;  // time elapsed since previous measurement
    uint32_t        previous; // previous measurement  
    uint32_t        dt;       // time step in ms
    MockInterrupt_t interrupt;// interrupt scan requests
} TestTiming_t;

/*
 Note that voltages are sent as dac codes
 */
typedef struct TestVoltage_s {
    uint8_t  initial;  // initial scan voltage
    uint8_t  final;    // final scan voltage
    uint8_t  dV;       // step size
    uint8_t  mock;     // mock voltage sent to dac
} TestVoltage_t;

/*
 Ideal diode iv data (see ShockleyData.py). Row index corresponds to the dac code.
 I_L     1.00
 I_0     1.00E-09
 V_T     0.0259
 R_S     0.00
 R_SH    inf
 n       1.00
*/
static double shockleyDiode[][3] = 
{
  // voltage , current     , power
    {0.000000, 1.000000e+00, 0.000000e+00},
    {0.008000, 1.000000e+00, 8.000000e-03},
    {0.016000, 1.000000e+00, 1.600000e-02},
    {0.024000, 1.000000e+00, 2.400000e-02},
    {0.032000, 1.000000e+00, 3.200000e-02},
    {0.040000, 1.000000e+00, 4.000000e-02},
    {0.048000, 1.000000e+00, 4.800000e-02},
    {0.056000, 1.000000e+00, 5.600000e-02},
    {0.064000, 1.000000e+00, 6.400000e-02},
    {0.072000, 1.000000e+00, 7.200000e-02},
    {0.080000, 1.000000e+00, 8.000000e-02},
    {0.088000, 1.000000e+00, 8.800000e-02},
    {0.096000, 1.000000e+00, 9.600000e-02},
    {0.104000, 9.999999e-01, 1.040000e-01},
    {0.112000, 9.999999e-01, 1.120000e-01},
    {0.120000, 9.999999e-01, 1.200000e-01},
    {0.128000, 9.999999e-01, 1.280000e-01},
    {0.136000, 9.999998e-01, 1.360000e-01},
    {0.144000, 9.999997e-01, 1.440000e-01},
    {0.152000, 9.999996e-01, 1.519999e-01},
    {0.160000, 9.999995e-01, 1.599999e-01},
    {0.168000, 9.999993e-01, 1.679999e-01},
    {0.176000, 9.999991e-01, 1.759998e-01},
    {0.184000, 9.999988e-01, 1.839998e-01},
    {0.192000, 9.999983e-01, 1.919997e-01},
    {0.200000, 9.999977e-01, 1.999995e-01},
    {0.208000, 9.999969e-01, 2.079994e-01},
    {0.216000, 9.999958e-01, 2.159991e-01},
    {0.224000, 9.999943e-01, 2.239987e-01},
    {0.232000, 9.999922e-01, 2.319982e-01},
    {0.240000, 9.999894e-01, 2.399975e-01},
    {0.248000, 9.999856e-01, 2.479964e-01},
    {0.256000, 9.999804e-01, 2.559950e-01},
    {0.264000, 9.999733e-01, 2.639929e-01},
    {0.272000, 9.999636e-01, 2.719901e-01},
    {0.280000, 9.999504e-01, 2.799861e-01},
    {0.288000, 9.999325e-01, 2.879806e-01},
    {0.296000, 9.999081e-01, 2.959728e-01},
    {0.304000, 9.998748e-01, 3.039619e-01},
    {0.312000, 9.998295e-01, 3.119468e-01},
    {0.320000, 9.997678e-01, 3.199257e-01},
    {0.328000, 9.996838e-01, 3.278963e-01},
    {0.336000, 9.995694e-01, 3.358553e-01},
    {0.344000, 9.994135e-01, 3.437983e-01},
    {0.352000, 9.992013e-01, 3.517189e-01},
    {0.360000, 9.989123e-01, 3.596084e-01},
    {0.368000, 9.985186e-01, 3.674548e-01},
    {0.376000, 9.979825e-01, 3.752414e-01},
    {0.384000, 9.972524e-01, 3.829449e-01},
    {0.392000, 9.962580e-01, 3.905331e-01},
    {0.400000, 9.949038e-01, 3.979615e-01},
    {0.408000, 9.930594e-01, 4.051682e-01},
    {0.416000, 9.905476e-01, 4.120678e-01},
    {0.424000, 9.871268e-01, 4.185418e-01},
    {0.432000, 9.824680e-01, 4.244262e-01},
    {0.440000, 9.761232e-01, 4.294942e-01},
    {0.448000, 9.674822e-01, 4.334320e-01},
    {0.456000, 9.557141e-01, 4.358056e-01},
    {0.464000, 9.396870e-01, 4.360148e-01},
    {0.472000, 9.178598e-01, 4.332298e-01},
    {0.480000, 8.881333e-01, 4.263040e-01},
    {0.488000, 8.476488e-01, 4.136526e-01},
    {0.496000, 7.925130e-01, 3.930865e-01},
    {0.504000, 7.174236e-01, 3.615815e-01},
    {0.512000, 6.151594e-01, 3.149616e-01},
    {0.520000, 4.758858e-01, 2.474606e-01},
    {0.528000, 2.862093e-01, 1.511185e-01},
    {0.536000, 2.788886e-02, 1.494843e-02},
    {0.544000, 0.000000e+00, 0.000000e+00},
    {0.552000, 0.000000e+00, 0.000000e+00},
    {0.560000, 0.000000e+00, 0.000000e+00},
    {0.568000, 0.000000e+00, 0.000000e+00},
    {0.576000, 0.000000e+00, 0.000000e+00},
    {0.584000, 0.000000e+00, 0.000000e+00},
    {0.592000, 0.000000e+00, 0.000000e+00},
    {0.600000, 0.000000e+00, 0.000000e+00},
    {0.608000, 0.000000e+00, 0.000000e+00},
    {0.616000, 0.000000e+00, 0.000000e+00},
    {0.624000, 0.000000e+00, 0.000000e+00},
    {0.632000, 0.000000e+00, 0.000000e+00},
    {0.640000, 0.000000e+00, 0.000000e+00},
    {0.648000, 0.000000e+00, 0.000000e+00},
    {0.656000, 0.000000e+00, 0.000000e+00},
    {0.664000, 0.000000e+00, 0.000000e+00},
    {0.672000, 0.000000e+00, 0.000000e+00},
    {0.680000, 0.000000e+00, 0.000000e+00},
    {0.688000, 0.000000e+00, 0.000000e+00},
    {0.696000, 0.000000e+00, 0.000000e+00},
    {0.704000, 0.000000e+00, 0.000000e+00},
    {0.712000, 0.000000e+00, 0.000000e+00},
    {0.720000, 0.000000e+00, 0.000000e+00},
    {0.728000, 0.000000e+00, 0.000000e+00},
    {0.736000, 0.000000e+00, 0.000000e+00},
    {0.744000, 0.000000e+00, 0.000000e+00},
    {0.752000, 0.000000e+00, 0.000000e+00},
    {0.760000, 0.000000e+00, 0.000000e+00},
    {0.768000, 0.000000e+00, 0.000000e+00},
    {0.776000, 0.000000e+00, 0.000000e+00},
    {0.784000, 0.000000e+00, 0.000000e+00},
    {0.792000, 0.000000e+00, 0.000000e+00},
    {0.800000, 0.000000e+00, 0.000000e+00},
    {0.808000, 0.000000e+00, 0.000000e+00},
    {0.816000, 0.000000e+00, 0.000000e+00},
    {0.824000, 0.000000e+00, 0.000000e+00},
    {0.832000, 0.000000e+00, 0.000000e+00},
    {0.840000, 0.000000e+00, 0.000000e+00},
    {0.848000, 0.000000e+00, 0.000000e+00},
    {0.856000, 0.000000e+00, 0.000000e+00},
    {0.864000, 0.000000e+00, 0.000000e+00},
    {0.872000, 0.000000e+00, 0.000000e+00},
    {0.880000, 0.000000e+00, 0.000000e+00},
    {0.888000, 0.000000e+00, 0.000000e+00},
    {0.896000, 0.000000e+00, 0.000000e+00},
    {0.904000, 0.000000e+00, 0.000000e+00},
    {0.912000, 0.000000e+00, 0.000000e+00},
    {0.920000, 0.000000e+00, 0.000000e+00},
    {0.928000, 0.000000e+00, 0.000000e+00},
    {0.936000, 0.000000e+00, 0.000000e+00},
    {0.944000, 0.000000e+00, 0.000000e+00},
    {0.952000, 0.000000e+00, 0.000000e+00},
    {0.960000, 0.000000e+00, 0.000000e+00},
    {0.968000, 0.000000e+00, 0.000000e+00},
    {0.976000, 0.000000e+00, 0.000000e+00},
    {0.984000, 0.000000e+00, 0.000000e+00},
    {0.992000, 0.000000e+00, 0.000000e+00},
    {1.000000, 0.000000e+00, 0.000000e+00},
    {1.008000, 0.000000e+00, 0.000000e+00},
    {1.016000, 0.000000e+00, 0.000000e+00},
    {1.024000, 0.000000e+00, 0.000000e+00},
    {1.032000, 0.000000e+00, 0.000000e+00},
    {1.040000, 0.000000e+00, 0.000000e+00},
    {1.048000, 0.000000e+00, 0.000000e+00},
    {1.056000, 0.000000e+00, 0.000000e+00},
    {1.064000, 0.000000e+00, 0.000000e+00},
    {1.072000, 0.000000e+00, 0.000000e+00},
    {1.080000, 0.000000e+00, 0.000000e+00},
    {1.088000, 0.000000e+00, 0.000000e+00},
    {1.096000, 0.000000e+00, 0.000000e+00},
    {1.104000, 0.000000e+00, 0.000000e+00},
    {1.112000, 0.000000e+00, 0.000000e+00},
    {1.120000, 0.000000e+00, 0.000000e+00},
    {1.128000, 0.000000e+00, 0.000000e+00},
    {1.136000, 0.000000e+00, 0.000000e+00},
    {1.144000, 0.000000e+00, 0.000000e+00},
    {1.152000, 0.000000e+00, 0.000000e+00},
    {1.160000, 0.000000e+00, 0.000000e+00},
    {1.168000, 0.000000e+00, 0.000000e+00},
    {1.176000, 0.000000e+00, 0.000000e+00},
    {1.184000, 0.000000e+00, 0.000000e+00},
    {1.192000, 0.000000e+00, 0.000000e+00},
    {1.200000, 0.000000e+00, 0.000000e+00},
    {1.208000, 0.000000e+00, 0.000000e+00},
    {1.216000, 0.000000e+00, 0.000000e+00},
    {1.224000, 0.000000e+00, 0.000000e+00},
    {1.232000, 0.000000e+00, 0.000000e+00},
    {1.240000, 0.000000e+00, 0.000000e+00},
    {1.248000, 0.000000e+00, 0.000000e+00},
    {1.256000, 0.000000e+00, 0.000000e+00},
    {1.264000, 0.000000e+00, 0.000000e+00},
    {1.272000, 0.000000e+00, 0.000000e+00},
    {1.280000, 0.000000e+00, 0.000000e+00},
    {1.288000, 0.000000e+00, 0.000000e+00},
    {1.296000, 0.000000e+00, 0.000000e+00},
    {1.304000, 0.000000e+00, 0.000000e+00},
    {1.312000, 0.000000e+00, 0.000000e+00},
    {1.320000, 0.000000e+00, 0.000000e+00},
    {1.328000, 0.000000e+00, 0.000000e+00},
    {1.336000, 0.000000e+00, 0.000000e+00},
    {1.344000, 0.000000e+00, 0.000000e+00},
    {1.352000, 0.000000e+00, 0.000000e+00},
    {1.360000, 0.000000e+00, 0.000000e+00},
    {1.368000, 0.000000e+00, 0.000000e+00},
    {1.376000, 0.000000e+00, 0.000000e+00},
    {1.384000, 0.000000e+00, 0.000000e+00},
    {1.392000, 0.000000e+00, 0.000000e+00},
    {1.400000, 0.000000e+00, 0.000000e+00},
    {1.408000, 0.000000e+00, 0.000000e+00},
    {1.416000, 0.000000e+00, 0.000000e+00},
    {1.424000, 0.000000e+00, 0.000000e+00},
    {1.432000, 0.000000e+00, 0.000000e+00},
    {1.440000, 0.000000e+00, 0.000000e+00},
    {1.448000, 0.000000e+00, 0.000000e+00},
    {1.456000, 0.000000e+00, 0.000000e+00},
    {1.464000, 0.000000e+00, 0.000000e+00},
    {1.472000, 0.000000e+00, 0.000000e+00},
    {1.480000, 0.000000e+00, 0.000000e+00},
    {1.488000, 0.000000e+00, 0.000000e+00},
    {1.496000, 0.000000e+00, 0.000000e+00},
    {1.504000, 0.000000e+00, 0.000000e+00},
    {1.512000, 0.000000e+00, 0.000000e+00},
    {1.520000, 0.000000e+00, 0.000000e+00},
    {1.528000, 0.000000e+00, 0.000000e+00},
    {1.536000, 0.000000e+00, 0.000000e+00},
    {1.544000, 0.000000e+00, 0.000000e+00},
    {1.552000, 0.000000e+00, 0.000000e+00},
    {1.560000, 0.000000e+00, 0.000000e+00},
    {1.568000, 0.000000e+00, 0.000000e+00},
    {1.576000, 0.000000e+00, 0.000000e+00},
    {1.584000, 0.000000e+00, 0.000000e+00},
    {1.592000, 0.000000e+00, 0.000000e+00},
    {1.600000, 0.000000e+00, 0.000000e+00},
    {1.608000, 0.000000e+00, 0.000000e+00},
    {1.616000, 0.000000e+00, 0.000000e+00},
    {1.624000, 0.000000e+00, 0.000000e+00},
    {1.632000, 0.000000e+00, 0.000000e+00},
    {1.640000, 0.000000e+00, 0.000000e+00},
    {1.648000, 0.000000e+00, 0.000000e+00},
    {1.656000, 0.000000e+00, 0.000000e+00},
    {1.664000, 0.000000e+00, 0.000000e+00},
    {1.672000, 0.000000e+00, 0.000000e+00},
    {1.680000, 0.000000e+00, 0.000000e+00},
    {1.688000, 0.000000e+00, 0.000000e+00},
    {1.696000, 0.000000e+00, 0.000000e+00},
    {1.704000, 0.000000e+00, 0.000000e+00},
    {1.712000, 0.000000e+00, 0.000000e+00},
    {1.720000, 0.000000e+00, 0.000000e+00},
    {1.728000, 0.000000e+00, 0.000000e+00},
    {1.736000, 0.000000e+00, 0.000000e+00},
    {1.744000, 0.000000e+00, 0.000000e+00},
    {1.752000, 0.000000e+00, 0.000000e+00},
    {1.760000, 0.000000e+00, 0.000000e+00},
    {1.768000, 0.000000e+00, 0.000000e+00},
    {1.776000, 0.000000e+00, 0.000000e+00},
    {1.784000, 0.000000e+00, 0.000000e+00},
    {1.792000, 0.000000e+00, 0.000000e+00},
    {1.800000, 0.000000e+00, 0.000000e+00},
    {1.808000, 0.000000e+00, 0.000000e+00},
    {1.816000, 0.000000e+00, 0.000000e+00},
    {1.824000, 0.000000e+00, 0.000000e+00},
    {1.832000, 0.000000e+00, 0.000000e+00},
    {1.840000, 0.000000e+00, 0.000000e+00},
    {1.848000, 0.000000e+00, 0.000000e+00},
    {1.856000, 0.000000e+00, 0.000000e+00},
    {1.864000, 0.000000e+00, 0.000000e+00},
    {1.872000, 0.000000e+00, 0.000000e+00},
    {1.880000, 0.000000e+00, 0.000000e+00},
    {1.888000, 0.000000e+00, 0.000000e+00},
    {1.896000, 0.000000e+00, 0.000000e+00},
    {1.904000, 0.000000e+00, 0.000000e+00},
    {1.912000, 0.000000e+00, 0.000000e+00},
    {1.920000, 0.000000e+00, 0.000000e+00},
    {1.928000, 0.000000e+00, 0.000000e+00},
    {1.936000, 0.000000e+00, 0.000000e+00},
    {1.944000, 0.000000e+00, 0.000000e+00},
    {1.952000, 0.000000e+00, 0.000000e+00},
    {1.960000, 0.000000e+00, 0.000000e+00},
    {1.968000, 0.000000e+00, 0.000000e+00},
    {1.976000, 0.000000e+00, 0.000000e+00},
    {1.984000, 0.000000e+00, 0.000000e+00},
    {1.992000, 0.000000e+00, 0.000000e+00},
    {2.000000, 0.000000e+00, 0.000000e+00},
    {2.008000, 0.000000e+00, 0.000000e+00},
    {2.016000, 0.000000e+00, 0.000000e+00},
    {2.024000, 0.000000e+00, 0.000000e+00},
    {2.032000, 0.000000e+00, 0.000000e+00},
    {2.040000, 0.000000e+00, 0.000000e+00}
};

// Calculated in -.py
static uint8_t mppCodeShockley = 58U;

/*******************************************************************************
 * Mock function implementations for tests
 ******************************************************************************/
/*
 Mock implemetation for Arduino library millis. Returns a long int corresponding
 to the current value of the device timer in ms.
 */
unsigned long millis(void)
{
    mock().actualCall("millis");
    uint32_t retval = mock().unsignedLongIntReturnValue();
    return retval;
}

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/
/*
 Manages simulated interrupt in IV scan step. Only applies when the max power
 point is being measured.
*/
static void InterruptScan(TestVoltage_t *v, TestTiming_t  *t)
{    
    if ((v->mock == mppCodeShockley)
        && (t->elapsed >= t->interrupt.start)
        && t->interrupt.requested)
    {
        t->mock += t->interrupt.duration;
        t->interrupt.requested = false;
    }
    t->elapsed = t->mock - t->previous;
}

/*
 Simulates an ideal diode under illumination connected to the lifetester. Returns
 an ADC code corresponding to the dac code input.
*/
static uint16_t TestGetAdcCodeForDiode(uint8_t dacCode)
{
    double adcCurrent = shockleyDiode[dacCode][currentData];
    uint16_t adcCode = (uint16_t)(adcCurrent * MAX_CURRENT * CURRENT_TO_CODE);
    return adcCode;
}

/*
 Simualtes a bad device connected to lifetester which delivers a fixed current
 with applied voltage. Power vs v will be linear ie. no hill to extract Mpp.
*/
static uint16_t TestGetAdcCodeConstantCurrent(uint8_t dacCode)
{
    dacCode;  // trying to avoid unused error
    return FIXED_CURRENT;
}
#if 0
/*
 Step function called repeatedly in tests to queue expected mock function calls
 from individual tests as v.mock is scanned. Requires a function pointer to
 return adc code from dac input. Gives flexibility of device behaviour.
*/
static void MocksForIvScanStep(LifeTester_t *const lifeTester,
                               TestTiming_t        *t,
                               TestVoltage_t       *v,
                               uint16_t (*GetAdcCode)(uint8_t))
{    
    // (1) Set voltage during settle time
    t->elapsed = t->mock - t->previous;
    printf("elapsed time %u\n", t->elapsed);
    if (t->elapsed < SETTLE_TIME)
    {
        printf("expecting dac set to %u\n", v->mock);
        mock().expectOneCall("DacSetOutput")
            .withParameter("output", v->mock)
            .withParameter("channel", lifeTester->io.dac);
    }
    // (2) Sample current during measurement time window
    else if((t->elapsed >= SETTLE_TIME) && (t->elapsed < (SETTLE_TIME + SAMPLING_TIME)))
    {
        // Mock returns shockley diode current from lookup table
        mock().expectOneCall("AdcReadData")
            .withParameter("channel", lifeTester->io.adc)
            .andReturnValue(GetAdcCode(v->mock));
    }
    // (3) Do calculations - update timer for next measurement
    else
    {
        mock().expectOneCall("millis").andReturnValue(t->mock);
        v->mock += v->dV;
        t->previous = t->mock;
    }

    t->mock += t->dt;
}
#endif
/*******************************************************************************
 * Unit tests
 ******************************************************************************/

static LifeTester_t *mockLifeTester;

// Define a test group for IV - all tests share common setup/teardown
TEST_GROUP(IVTestGroup)
{
    // Functions called after every test
    void setup(void)
    {
        mock().disable();

        // Initialised data
        const LifeTester_t lifetesterSetup = {
            {chASelect, 0U},    // io
            Flasher(LED_A_PIN), // led
            {0},                // data
            NULL,               // state function
            0U,                 // timer
            ok                  // error
        };
        // Copy to a static variable for tests to work on
        static LifeTester_t lifeTesterForTest = lifetesterSetup;
        // Need to copy the data every time. A static is only initialised once.
        memcpy(&lifeTesterForTest, &lifetesterSetup, sizeof(LifeTester_t));
        // Access with a pointer from tests
        mockLifeTester = &lifeTesterForTest;

        mock().enable();
    }

    // Functions called after every test
    void teardown(void)
    {
        mock().clear();
    }
};
#if 0
/*
 Test that updating the state machine while in scanning mode set dac state sets
 the dac when not set and when within the settle time.
*/
TEST(IVTestGroup, UpdatingScanInSetDacStateSettleTimeSetsDacWhenNotSet)
{
    const uint32_t tPrevious = 239348U;               // time last measurement was made 
    const uint32_t tElapsed = 10U;                    // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 64U;                       // present scan voltage

    mockLifeTester->nextState = StateSetToScanVoltage;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->data.vScan = vMock; // initialise with a non-zero voltage

    // store data in expect variable
    LifeTester_t lifeTesterExp = *mockLifeTester;
    // expect no change in mode

    mock().expectOneCall("millis").andReturnValue(tMock);

    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock)
        .withParameter("channel", mockLifeTester->io.dac);

    // The dac shouldn't be set before the update is called
    CHECK(!DacOutputSetToScanVoltage(mockLifeTester));    
    IV_ScanUpdate(mockLifeTester);
    // mode not expected to change
    POINTERS_EQUAL(lifeTesterExp.nextState, mockLifeTester->nextState);    
    // no other data expected to change either
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));
    // Dac should be set now.
    CHECK(DacOutputSetToScanVoltage(mockLifeTester));

    /*Now the dac is set, if we call the update function again, mode should change
    without any call to set the dac. */
    lifeTesterExp.nextState = StateSampleScanCurrent;
    mock().expectOneCall("millis").andReturnValue(tMock);

    IV_ScanUpdate(mockLifeTester);
    POINTERS_EQUAL(lifeTesterExp.nextState, mockLifeTester->nextState);    
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));

    mock().checkExpectations();
}

/*
 Test that updating the state machine while in scanning mode set dac state changes
 state to sample current if the settle time has expired.
*/
TEST(IVTestGroup, UpdatingScanInSetDacStateAfterSettleTimeChangesMode)
{
    const uint32_t tPrevious = 9348U;                 // time last measurement was made 
    const uint32_t tElapsed = 10U + SETTLE_TIME;      // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis

    mockLifeTester->nextState = StateSetToScanVoltage;
    mockLifeTester->timer = tPrevious;

    // store data in expect variable
    LifeTester_t lifeTesterExp = *mockLifeTester;
    // expect mode to change
    lifeTesterExp.nextState = StateSampleScanCurrent;

    // Only expect the time to be checked. No need to actually set dac.
    mock().expectOneCall("millis").andReturnValue(tMock);

    IV_ScanUpdate(mockLifeTester);
    // mode is expected to change
    POINTERS_EQUAL(lifeTesterExp.nextState, mockLifeTester->nextState);    
    // no other data expected to change either
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));
    
    mock().checkExpectations();
}

/*
 Test that updating the state machine while in sample scan current takes a
 measurement provided that time is within sample time and that the dac has been
 set to the scan voltage. Several measurements are taken. Expect an average.
*/
TEST(IVTestGroup, UpdateScanInSampleScanCurrentAdcMeasurementExpected)
{
    const uint32_t nMeasurements = 3U;

    const uint32_t tPrevious = 9348U;
    const uint32_t tMock[] =
    {
        tPrevious + SAMPLING_TIME + 10U,
        tPrevious + SAMPLING_TIME + 23U,
        tPrevious + SAMPLING_TIME + 76U
    };

    const uint32_t vMock = 52U;
    
    // queue up some current measurements
    const uint32_t iMock[] = {3487U, 2435U, 3488U};
    const uint32_t iMockSum = (iMock[0] + iMock[1] + iMock[2]);
    const uint32_t iMockAve = iMockSum / nMeasurements;

    // setup
    mockLifeTester->nextState = StateSampleScanCurrent;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->data.vScan = vMock;
    mock().disable();
    DacSetOutputToScanVoltage(mockLifeTester);
    mock().enable();
    
    // Setup expect variable. Don't expect mode to change
    LifeTester_t lifeTesterExp = *mockLifeTester;
    // ...only measurement stuff
    lifeTesterExp.data.iScan = iMockAve;
    lifeTesterExp.data.iScanSum = iMockSum;
    lifeTesterExp.data.nReadsScan = nMeasurements;

    for (int i = 0; i < nMeasurements; i++)
    {
        mock().expectOneCall("millis").andReturnValue(tMock[i]);
        mock().expectOneCall("AdcReadData")
            .withParameter("channel", mockLifeTester->io.adc)
            .andReturnValue(iMock[i]);
        IV_ScanUpdate(mockLifeTester);
    }

    CHECK_EQUAL(iMockAve, mockLifeTester->data.iScan);
    CHECK_EQUAL(iMockSum, mockLifeTester->data.iScanSum);
    CHECK_EQUAL(nMeasurements, mockLifeTester->data.nReadsScan);
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));
    mock().checkExpectations();
}

/*
 In sample scan current, the adc should not be measured if the dac hasn't been
 set. Instead, the measurement should be restarted by resetting the timer and
 going back to set dac state.
*/
TEST(IVTestGroup, UpdateScanInSampleScanCurrentDacNotSetDontReadAdc)
{
    const uint32_t tPrevious = 9348U;                 // time last measurement was made 
    const uint32_t tElapsed = 10U + SETTLE_TIME;      // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 34U;

    /* Deliberately set the dac to the wrong voltage. Ensures that the function
    under test actually checks the voltage before adc reading. */
    mock().disable();
    DacSetOutput((vMock - 1U), mockLifeTester->io.dac);
    mock().enable();
    mockLifeTester->data.vScan = vMock;
    mockLifeTester->nextState = StateSampleScanCurrent;
    
    LifeTester_t lifeTesterExp = *mockLifeTester;
    lifeTesterExp.nextState = StateSetToScanVoltage;
    lifeTesterExp.timer = tMock;
    mock().expectOneCall("millis").andReturnValue(tMock);
    // nb. adc read call not expected.
    IV_ScanUpdate(mockLifeTester);
    POINTERS_EQUAL(StateSetToScanVoltage, mockLifeTester->nextState);
    CHECK_EQUAL(tMock, mockLifeTester->timer);
    mock().checkExpectations();
}

/*
 In sample scan current, the adc should not be measured because the time is outside
 the sampling window. Instead mode should change to analyse measurement.
*/
TEST(IVTestGroup, UpdateScanInSampleScanCurrentPastSampleTimeChangeMode)
{
    const uint32_t tPrevious = 9348U;
    const uint32_t tElapsed = 10U + SETTLE_TIME + SAMPLING_TIME;
    const uint32_t tMock = tPrevious + tElapsed;
    const uint32_t vMock = 34U;

    mock().disable();
    DacSetOutput(vMock, mockLifeTester->io.dac);
    mock().enable();
    mockLifeTester->data.vScan = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateSampleScanCurrent;
    
    LifeTester_t lifeTesterExp = *mockLifeTester;
    lifeTesterExp.nextState = StateAnalyseScanMeasurement;
    mock().expectOneCall("millis").andReturnValue(tMock);
    // nb. adc read call not expected.
    IV_ScanUpdate(mockLifeTester);
    POINTERS_EQUAL(StateAnalyseScanMeasurement, mockLifeTester->nextState);
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));
    mock().checkExpectations();
}

/*
 Max power point calculation not done becuase adc hasn't been measured. Expect 
 to reset relevant variables and go back to dac set state without changing voltage.
*/
TEST(IVTestGroup, UpdateScanInAnalyseMeasurementNoDacReadingExpectReset)
{
    const uint32_t tPrevious = 9348U;                 // time last measurement was made 
    const uint32_t tElapsed =
        10U + SETTLE_TIME + SAMPLING_TIME;            // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 34U;

    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateAnalyseScanMeasurement;
    mockLifeTester->data.vScan = vMock;
    mockLifeTester->data.nReadsScan = 0U;  // no adc readings - so can't calculate
    mockLifeTester->data.iScan = 243U;     // set some data to be reset
    mockLifeTester->data.iScanSum = 2343U;

    LifeTester_t lifeTesterExp = *mockLifeTester;

    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_ScanUpdate(mockLifeTester);

    CHECK_EQUAL(StateSetToScanVoltage, mockLifeTester->nextState);
    CHECK_EQUAL(vMock, mockLifeTester->data.vScan);
    CHECK_EQUAL(tMock, mockLifeTester->timer);
    CHECK_EQUAL(0U, mockLifeTester->data.iScanSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iScan);
    CHECK_EQUAL(0U, mockLifeTester->data.nReadsScan);

    mock().checkExpectations();
}

/*
 Max power point calculation done but new point is not the maximum so far. Expect
 current max power point to stay the same, meaurement to be reset, voltage to 
 increment mode to change back to set dac state.
*/
TEST(IVTestGroup, UpdateScanInAnalyseMeasurementNoNewMPP)
{
    const uint32_t tPrevious = 9348U;                 // time last measurement was made 
    const uint32_t tElapsed =
        10U + SETTLE_TIME + SAMPLING_TIME;            // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 34U;
    const uint32_t vMppExpected = 30U;
    const uint32_t iMock = 243;
    const uint32_t iMockSum = 2343U;
    const uint32_t iMaxExpected = iMock * 2U;
    const uint32_t pScanExpected = vMock * iMock;
    const uint32_t pScanMppExpected = pScanExpected + 1U;

    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateAnalyseScanMeasurement;
    mockLifeTester->data.vScan = vMock;
    mockLifeTester->data.vScanMpp = vMppExpected;
    mockLifeTester->data.nReadsScan = 2U;
    mockLifeTester->data.iScan = iMock;     // set some data to be reset
    mockLifeTester->data.iScanSum = iMockSum;
    mockLifeTester->data.iScanMpp = iMaxExpected;
    mockLifeTester->data.pScanMpp = pScanMppExpected;
    mockLifeTester->data.pScan = 0U;        // set to 0 to check update

    LifeTester_t lifeTesterExp = *mockLifeTester;

    // call function under test
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_ScanUpdate(mockLifeTester);

    CHECK_EQUAL(StateSetToScanVoltage, mockLifeTester->nextState);
    CHECK_EQUAL(vMock + DV_SCAN, mockLifeTester->data.vScan);

    // variables reset for next measurement
    CHECK_EQUAL(tMock, mockLifeTester->timer);
    CHECK_EQUAL(0U, mockLifeTester->data.iScanSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iScan);
    CHECK_EQUAL(0U, mockLifeTester->data.nReadsScan);
    // pScan expected to be updated but not mpp
    CHECK_EQUAL(pScanExpected, mockLifeTester->data.pScan);
    CHECK_EQUAL(pScanMppExpected, mockLifeTester->data.pScanMpp);
    CHECK_EQUAL(vMppExpected, mockLifeTester->data.vScanMpp);
    CHECK_EQUAL(iMaxExpected, mockLifeTester->data.iScanMpp);

    mock().checkExpectations();
}

/*
 Max power point calculation done and new point is a new mpp. Expect mpp value to
 be updated with the present measurement, meaurement to be reset, voltage to 
 increment mode to change back to set dac state.
*/
TEST(IVTestGroup, UpdateScanInAnalyseMeasurementWithNewMPP)
{
    const uint32_t tPrevious = 9348U;                 // time last measurement was made 
    const uint32_t tElapsed =
        10U + SETTLE_TIME + SAMPLING_TIME;            // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 34U;
    const uint32_t vMppOld = 30U;
    const uint32_t iNew = 243;
    const uint32_t iNewSum = 2343U;
    const uint32_t iMaxOld = 34;

    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateAnalyseScanMeasurement;
    mockLifeTester->data.vScan = vMock;
    mockLifeTester->data.vScanMpp = vMppOld;
    mockLifeTester->data.nReadsScan = 2U;
    mockLifeTester->data.iScan = iNew;     // set some data to be reset
    mockLifeTester->data.iScanSum = iNewSum;
    mockLifeTester->data.iScanMpp = iMaxOld;
    mockLifeTester->data.pScanMpp = iMaxOld * vMppOld;
    mockLifeTester->data.pScan = 0U;        // set to 0 to check update

    LifeTester_t lifeTesterExp = *mockLifeTester;

    // call function under test
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_ScanUpdate(mockLifeTester);

    CHECK_EQUAL(StateSetToScanVoltage, mockLifeTester->nextState);
    CHECK_EQUAL(vMock + DV_SCAN, mockLifeTester->data.vScan);

    // variables reset for next measurement
    CHECK_EQUAL(tMock, mockLifeTester->timer);
    CHECK_EQUAL(0U, mockLifeTester->data.iScanSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iScan);
    CHECK_EQUAL(0U, mockLifeTester->data.nReadsScan);
    // Expect powers to be updated with the new mpp
    CHECK_EQUAL(iNew * vMock, mockLifeTester->data.pScan);
    // CHECK_EQUAL(iNew * vMock, mockLifeTester->data.pScanMpp);
    CHECK_EQUAL(vMock, mockLifeTester->data.vScanMpp);
    CHECK_EQUAL(iNew, mockLifeTester->data.iScanMpp);

    mock().checkExpectations();
}

/*
 IV scan function should not be called if the lifetester object is in it's error
 state. The lifetester data should not change in this case.
*/
TEST(IVTestGroup, RunIvScanErrorStateExpectDontScan)
{
    // already in error state. Function shouldn't run.
    mockLifeTester->error = currentLimit;

    // copy initial data and pass mock to function under test
    LifeTester_t lifeTesterExp = *mockLifeTester;
    lifeTesterExp.nextState = StateSetToScanVoltage;
    // Note that voltages are sent as dac codes
    const uint8_t vInitial = 45U;
    const uint8_t vFinal   = 70U;
    const uint8_t dV       = 1U;

    // Call function under test
    IV_ScanAndUpdate(mockLifeTester, vInitial, vFinal, dV);
    // Check that the lifetester data hasn't changed
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));

    // No mocks should be called
    mock().checkExpectations();
}
#endif
/*
 Test for running through an iv scan without errors. Expect to return the mpp 
 of a shockley diode.
*/
TEST(IVTestGroup, RunIvScanNoErrorExpectDiodeMppReturned)
{    
    uint32_t tMock = 9348U;          // time last measurement was made 
    const uint32_t dt = 50U;
    uint32_t tPrevious = tMock;
    mockLifeTester->nextState = StateInitialise;

    // start in initialise state - timer should be set
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_ScanUpdate(mockLifeTester);
    POINTERS_EQUAL(StateSetToScanVoltage, mockLifeTester->nextState);
    CHECK_EQUAL(tPrevious, mockLifeTester->timer);
    
    uint32_t vExpect = V_SCAN_MIN;
    // increment time a little
    tMock += dt;
    // keep executing scan loop until finished.
    while (vExpect < V_SCAN_MAX)
    {
        // (1) Expect to set voltage during settle time if it isn't already
        POINTERS_EQUAL(StateSetToScanVoltage, mockLifeTester->nextState);
        // timer should be zeroed as soon as dac is set for the measurement
        if (!DacOutputSetToScanVoltage(mockLifeTester))
        {
            mock().expectOneCall("DacSetOutput")
                .withParameter("output", vExpect)
                .withParameter("channel", mockLifeTester->io.dac);
            mock().expectOneCall("millis").andReturnValue(tMock);
            tPrevious = tMock;
        }
        IV_ScanUpdate(mockLifeTester);
        CHECK(DacOutputSetToScanVoltage(mockLifeTester));
        CHECK_EQUAL(tPrevious, mockLifeTester->timer);

        // (2) Expect to sample current during measurement time window
        POINTERS_EQUAL(StateSampleScanCurrent, mockLifeTester->nextState);
        tMock += SETTLE_TIME;
        // Mock returns shockley diode current from lookup table
        mock().expectOneCall("millis").andReturnValue(tMock);
        mock().expectOneCall("AdcReadData")
            .withParameter("channel", mockLifeTester->io.adc)
            .andReturnValue(TestGetAdcCodeForDiode(vExpect));
        IV_ScanUpdate(mockLifeTester);

        // Sampling time expired. Expect to transition to analyse measurement.
        tMock += SAMPLING_TIME;
        mock().expectOneCall("millis").andReturnValue(tMock);
        IV_ScanUpdate(mockLifeTester);
        POINTERS_EQUAL(StateAnalyseScanMeasurement, mockLifeTester->nextState);
        
        /*(3) Expect to do calculations and check the power point for new mpp
        Not expecting timer to be read here - only calculations*/
        IV_ScanUpdate(mockLifeTester);
        vExpect += DV_SCAN;

        printf("mock time = %u elapsed = %u\n", tMock, tMock - tPrevious);
        mock().checkExpectations();
    }
    // Expect the result to be the calculated mpp.
    CHECK_EQUAL(mppCodeShockley, mockLifeTester->data.vScanMpp);
}

#if 1
/*
 Test for updating mpp during settle time. Expect the dac to be set only. No
 calls to read adc and no change to lifetester data.
*/
TEST(IVTestGroup, UpdateMppDuringSettleTime)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME + 10U; // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis

    mockLifeTester->timer = tPrevious;
    // set lifetester state
    mockLifeTester->nextState = StateSetToThisVoltage;

    // copy data for comparison later - shouldn't change due to function call
    const LifeTester_t lifeTesterExp = *mockLifeTester;

    // 1) check time. millis should return time within settle time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    
    // 2) therefore expect dac to be set
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", mockLifeTester->data.vThis)
        .withParameter("channel", mockLifeTester->io.dac);

    IV_MpptUpdate(mockLifeTester);
    // Expect no change to lifetester data
    MEMCMP_EQUAL(&lifeTesterExp, mockLifeTester, sizeof(LifeTester_t));
}

/*
 Test for updating mpp during track delay. Expect nothing to happen here. Delay time
 inserted to ensure that we aren't trying to update mpp too often.
*/
TEST(IVTestGroup, UpdateMppDuringTrackDelayTime)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = 10U;                    // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis

    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateWaitForTrackingDelay;

    // Check time. millis should return time within tracking delay time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Call function under test
    const LifeTester_t lifeTesterExp = *mockLifeTester;
    IV_MpptUpdate(mockLifeTester);
    const LifeTester_t lifeTesterActual = *mockLifeTester;
    // Expect no change to lifetester data
    MEMCMP_EQUAL(&lifeTesterExp, &lifeTesterActual, sizeof(LifeTester_t));
}

/*
 Test for updating mpp during settle time. Expect the dac to be set only. No
 calls to read adc and no change to lifetester data.
*/
TEST(IVTestGroup, UpdateMppDuringSettleTimeForThisPoint)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME + 10U; // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 43U;

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateSetToThisVoltage;

    // 1) check time. millis should return time within settle time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    
    // 2) therefore expect dac to be set
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock)
        .withParameter("channel", mockLifeTester->io.dac);

    const LifeTester_t lifeTesterExp = *mockLifeTester;
    IV_MpptUpdate(mockLifeTester);
    const LifeTester_t lifeTesterActual = *mockLifeTester;
    // Expect no change to lifetester data
    MEMCMP_EQUAL(&lifeTesterExp, &lifeTesterActual, sizeof(LifeTester_t));
}

/*
 Test for updating mpp during sampling time window. Expect the adc to be read and
 relevant data in the lifetester object to be updated.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForThisPoint)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME
                              + SETTLE_TIME +  10U;   // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateSampleThisCurrent;

    /*Need to set the mock dac hardware to this voltage otherwise the lifetester
    won't actually sample any current. Measurement will be reset. Do this 
    outside of mocking because this is setup and not actual behaviour of function.*/
    mock().disable();
    DacSetOutputToThisVoltage(mockLifeTester);
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    mock().enable();

    // Check time. millis should return time within sampling time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Expect call to adc since we're in the current sampling window
    const uint16_t iMock = TestGetAdcCodeForDiode(vMock);
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", mockLifeTester->io.adc)
        .andReturnValue(iMock);

    // Call function under test
    LifeTester_t lifeTesterActual = *mockLifeTester;
    IV_MpptUpdate(&lifeTesterActual);

    // expect the current and number of readings to be updated only
    LifeTester_t lifeTesterExpected = *mockLifeTester;
    lifeTesterExpected.data.iThisSum = iMock;
    lifeTesterExpected.data.nReadsThis++;
    MEMCMP_EQUAL(&lifeTesterExpected, &lifeTesterActual, sizeof(LifeTester_t));
}

/*
 Test for updating mpp during sampling time window. The adc should not be read.
 Instead the dac output should be checked and when found not to be correct, the 
 measurement should be reset.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForThisPointDacNotSet)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME
                              + SETTLE_TIME +  10U;   // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateSampleThisCurrent;

    /*Ensure that the dac is not set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutput(vMock - 10U, mockLifeTester->io.dac);
    CHECK(!DacOutputSetToThisVoltage(mockLifeTester));
    mock().enable();

    // Check time. millis should return time within sampling time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Expect another call to millis to reset the measurement timer
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Call function under test
    LifeTester_t lifeTesterActual = *mockLifeTester;
    IV_MpptUpdate(&lifeTesterActual);

    // expect the timer to be updated and mode to change
    LifeTester_t lifeTesterExpected = *mockLifeTester;
    lifeTesterExpected.timer = tMock;
    lifeTesterExpected.nextState = StateWaitForTrackingDelay;
    MEMCMP_EQUAL(&lifeTesterExpected, &lifeTesterActual, sizeof(LifeTester_t));
}

/*
 Test for updating mpp during second settle time ie. after current point has been
 measured. Next point is to be measured now for comparison. Expect the dac to be
 set only - to v + dV. No calls to read adc and no change to lifetester data.
*/
TEST(IVTestGroup, UpdateMppDuringSettleTimeForNextPoint)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME
                              + SETTLE_TIME
                              + SAMPLING_TIME + 10U;  // time elapsed since
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 43U;

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateSetToNextVoltage;

    // 1) check time. millis should return time within settle time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    
    // 2) therefore expect dac to be set
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock + DV_MPPT)
        .withParameter("channel", mockLifeTester->io.dac);

    const LifeTester_t lifetesterExp = *mockLifeTester;
    IV_MpptUpdate(mockLifeTester);
    const LifeTester_t lifetesterActual = *mockLifeTester;
    // Expect no change to lifetester data
    MEMCMP_EQUAL(&lifetesterExp, &lifetesterActual, sizeof(LifeTester_t));
}

/*
 Test for updating mpp during the second sampling time window. Expect the adc to
 be read and relevant data in the lifetester object corresponding to the next
 point to be updated.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForNextPoint)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME        // set elapsed time to next sample window
                              + (2 * SETTLE_TIME)
                              + SAMPLING_TIME + 10U;  
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point

    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateSampleNextCurrent;

    mock().disable();
    DacSetOutput(vMock + DV_MPPT, mockLifeTester->io.dac);
    CHECK(DacOutputSetToNextVoltage(mockLifeTester));
    mock().enable();

    // Check time. millis should return time within sampling time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Expect call to adc since we're in the current sampling window
    const uint16_t iMock = TestGetAdcCodeForDiode(vMock + DV_MPPT);
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", mockLifeTester->io.adc)
        .andReturnValue(iMock);

    // Call function under test
    LifeTester_t lifeTesterActual = *mockLifeTester;
    IV_MpptUpdate(&lifeTesterActual);

    // expect the current and number of readings to be updated only
    LifeTester_t lifeTesterExpected = *mockLifeTester;
    lifeTesterExpected.data.iNextSum = iMock;
    lifeTesterExpected.data.nReadsNext++;
    MEMCMP_EQUAL(&lifeTesterExpected, &lifeTesterActual, sizeof(LifeTester_t));
}

/*
 Test for updating mpp after all measurements complete. Data stored in lifetester
 object. Expect function to calculate which point provides the greatest power
 and set the stored value of voltage accordingly.
*/
TEST(IVTestGroup, UpdateMppAfterMeasurementsIncreaseVoltage)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME        // set elapsed time past...
                              + (2 * SETTLE_TIME)     // the measurement windows
                              + (2 * SAMPLING_TIME) + 10U;  
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    const uint16_t nAdcSamples = 4U;
    const uint16_t iThisSum = MIN_CURRENT * nAdcSamples;     // ensure that the next point has more power
    const uint16_t iNextSum = (MIN_CURRENT + 20U) * nAdcSamples;

    const uint32_t pThis = iThisSum / nAdcSamples * vMock;
    const uint32_t pNext = iNextSum / nAdcSamples * (vMock + DV_MPPT);
    CHECK(pNext > pThis);

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->data.iThisSum = iThisSum;
    mockLifeTester->data.iNextSum = iNextSum;
    mockLifeTester->data.nReadsThis = nAdcSamples;
    mockLifeTester->data.nReadsNext = nAdcSamples;
    mockLifeTester->nextState = StateAnalyseMeasurement;

    // Check time. millis should return time within sampling time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Expect call to change led state - indicates increase in voltage
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 2);
    // Expect another call to millis to update the value of the timmer in lifetester object
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Call function under test
    IV_MpptUpdate(mockLifeTester);

    // Exoect that working voltage should increase
    CHECK_EQUAL(vMock + DV_MPPT, mockLifeTester->data.vThis);
    // that timer should be updated since measurement is done
    CHECK_EQUAL(tMock, mockLifeTester->timer);
    // check there's no error
    CHECK_EQUAL(ok, mockLifeTester->error);
    // State should change now that measurements are done
    CHECK_EQUAL(StateWaitForTrackingDelay, mockLifeTester->nextState);
}

/*
 Test for updating mpp after all measurements complete. Data stored in lifetester
 object. Expect function to determine that the current point has more power so
 it should decide to decrement the working voltage. 
*/
TEST(IVTestGroup, UpdateMppAfterMeasurementsDecreaseVoltage)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME        // set elapsed time past...
                              + (2 * SETTLE_TIME)     // the measurement windows
                              + (2 * SAMPLING_TIME) + 10U;  
    const uint32_t tMock = tPrevious + tElapsed;      // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    const uint16_t nAdcSamples = 4U;
    const uint16_t iThisSum = (MIN_CURRENT + 20U) * nAdcSamples;
    const uint16_t iNextSum = MIN_CURRENT * nAdcSamples;     // ensure that the next point has more power

    const uint32_t pCurrent = iThisSum / nAdcSamples * vMock;
    const uint32_t pNext = iNextSum / nAdcSamples * (vMock + DV_MPPT);
    CHECK(pNext < pCurrent);

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->data.iThisSum = iThisSum;
    mockLifeTester->data.iNextSum = iNextSum;
    mockLifeTester->data.nReadsThis = nAdcSamples;
    mockLifeTester->data.nReadsNext = nAdcSamples;
    mockLifeTester->nextState = StateAnalyseMeasurement;

    // Check time. millis should return time within sampling time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Expect call to change led state - indicates decrease in voltage
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 1);
    // Expect another call to millis to update the value of the timmer in lifetester object
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Call function under test
    IV_MpptUpdate(mockLifeTester);

    // Exoect that working voltage should reduce
    CHECK_EQUAL(vMock - DV_MPPT, mockLifeTester->data.vThis);
    // that timer should be updated since measurement is done
    CHECK_EQUAL(tMock, mockLifeTester->timer);
    // expect low current error here since iNext and iCurrent are very low
    CHECK_EQUAL(ok, mockLifeTester->error);
    // State should change now that measurements are done
    CHECK_EQUAL(StateWaitForTrackingDelay, mockLifeTester->nextState);
}

/*
 Test for state transitions from initial tracking delay to set voltage and then
 to sample current etc for this and next measurement.
*/
TEST(IVTestGroup, IndividualStateTransitionsFromTrackingDelayOnwards)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME + 1U;  // set elapsed time in settle time
    uint32_t tMock = tPrevious + tElapsed;      // value to return from millis

    const uint32_t vMock = 4U;

    mockLifeTester->timer = tPrevious;
    mockLifeTester->nextState = StateWaitForTrackingDelay;
    mockLifeTester->data.vThis = vMock;

    // Time checked by update function
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Call function under test
    IV_MpptUpdate(mockLifeTester);
    // Expect change in state because time exceeds tracking delay
    CHECK_EQUAL(StateSetToThisVoltage, mockLifeTester->nextState);

    // Update again. Setting voltage but no time elapsed so no state transition
    mock().expectOneCall("millis").andReturnValue(tMock);
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock)
        .withParameter("channel", mockLifeTester->io.dac);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSetToThisVoltage, mockLifeTester->nextState);

    // Update again with elapsed time. Expect another transition.
    tMock += SETTLE_TIME;
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSampleThisCurrent, mockLifeTester->nextState);

    /*Update again in the sampling window. Expect current to be measured but no
    transition should happen*/
    mock().expectOneCall("millis").andReturnValue(tMock);    
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", mockLifeTester->io.adc)
        .andReturnValue(3245U);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSampleThisCurrent, mockLifeTester->nextState);

    /* Increment time into the second settle time. Expect transition. */
    tMock += SAMPLING_TIME;
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSetToNextVoltage, mockLifeTester->nextState);

    /* Expect the voltage to be set to the next point. No state change. */
    mock().expectOneCall("millis").andReturnValue(tMock);
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock + DV_MPPT)
        .withParameter("channel", mockLifeTester->io.dac);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSetToNextVoltage, mockLifeTester->nextState);

    /* Increment timer again and expect a state change to sample next current. */
    tMock += SETTLE_TIME;
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSampleNextCurrent, mockLifeTester->nextState);

    /* In current measure state. Expect adc measurement no transition */
    mock().expectOneCall("millis").andReturnValue(tMock);    
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", mockLifeTester->io.adc)
        .andReturnValue(3245U);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateSampleNextCurrent, mockLifeTester->nextState);

    /* Sampling done. Expect transition to measurement analysis */
    tMock += SAMPLING_TIME;
    mock().expectOneCall("millis").andReturnValue(tMock);
    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateAnalyseMeasurement, mockLifeTester->nextState);

    /* Updateing again should restart the whole measurement process */
    mock().expectOneCall("millis").andReturnValue(tMock);
    // two led flashes requested since power has increased
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 2);
    // second call expected when timer is reset
    mock().expectOneCall("millis").andReturnValue(tMock);

    IV_MpptUpdate(mockLifeTester);
    CHECK_EQUAL(StateWaitForTrackingDelay, mockLifeTester->nextState);

}
#endif