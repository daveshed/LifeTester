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

// Calculated in SchockleyData.py
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
 Manages simulated interrupt in IV scan step.
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

/*
 Step function called repeatedly in tests to queue expected mock function calls
 from individual tests as v.mock is scanned. Requires a function pointer to
 return adc code from dac input. Gives flexibility of device behaviour.
*/
static void MocksForIvScanStep(LifeTester_t *const lifetester,
                               TestTiming_t        *t,
                               TestVoltage_t       *v,
                               uint16_t (*GetAdcCode)(uint8_t))
{    
    // Setup
    mock().expectOneCall("millis").andReturnValue(t->mock);
    mock().expectOneCall("Flasher::update");

    // (1) Set voltage during settle time
    t->elapsed = t->mock - t->previous;
    if (t->elapsed < SETTLE_TIME)
    {
        mock().expectOneCall("DacSetOutput")
            .withParameter("output", v->mock)
            .withParameter("channel", lifetester->io.dac);
    }
    // (2) Sample current during measurement time window
    else if((t->elapsed >= SETTLE_TIME) && (t->elapsed < (SETTLE_TIME + SAMPLING_TIME)))
    {
        // Mock returns shockley diode current from lookup table
        mock().expectOneCall("AdcReadData")
            .withParameter("channel", lifetester->io.adc)
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

/*
 Test to run an IV scan and search for the maximum power point on a model device
 (shockley diode - https://en.wikipedia.org/wiki/Shockley_diode_equation.
 Function under test should implement a step function in a while loop that:
 1) sets voltage during settle time
 2) then samples current in the sampling window.
 3) It should then check the power obtained from the device and update the MPP 
 voltage.
 4) Finally, sets the lifetester voltage to the MPP.
*/
TEST(IVTestGroup, RunIvScanFindsMpp)
{
    TestTiming_t t;
    t.initial = 348775U;
    t.mock = t.initial;
    t.previous = t.mock;
    t.dt = 100U;

    // IV scan setup - mock calls that happen prior to IV scan loop
    mock().expectOneCall("millis").andReturnValue(t.initial);
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    
    TestVoltage_t v;
    v.initial = 45U;        // initial scan voltage
    v.final   = 70U;        // final scan voltage
    v.dV      = 1U;         // step size
    v.mock    = v.initial;  // mock voltage sent to dac

    // IV scan loop - queue mocks ready for calls by function under test
    while(v.mock <= v.final)
    {
        MocksForIvScanStep(mockLifeTester, &t, &v, TestGetAdcCodeForDiode);
    }
    
    // Reset dac after measurements
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", mppCodeShockley)
        .withParameter("channel", mockLifeTester->io.dac);

    // Call function under test
    IV_ScanAndUpdate(mockLifeTester, v.initial, v.final, v.dV);

    // Test assertions. Does the max power point agree with expectations
    // mpp dac code stored in lifetester instance
    const uint8_t mppCodeActual = mockLifeTester->data.vThis;
    CHECK_EQUAL(mppCodeShockley, mppCodeActual);
    // expect no errors
    CHECK_EQUAL(ok, mockLifeTester->error);
    CHECK_EQUAL(0, mockLifeTester->data.nErrorReads);
    
    // Check mock function calls match expectations
    mock().checkExpectations();
}

/*
 IV scan shape is checked and error raised if it's not a hill shape. The first
 and last points are compared against the maximum power point - mpp should be
 highest. Here, the lifetester measures a linear power trace due to a constant
 current. 
*/
TEST(IVTestGroup, RunIvScanInvalidScanNotHillShaped)
{
    const uint32_t initV = 11U;
    mockLifeTester->data.vThis = initV;

    TestTiming_t t;
    t.initial = 348775U;
    t.mock = t.initial;
    t.previous = t.mock;
    t.dt = 100U;

    // IV scan setup - mock calls that happen prior to IV scan loop
    mock().expectOneCall("millis").andReturnValue(t.initial);
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    
    TestVoltage_t v;
    v.initial = 45U;        // initial scan voltage
    v.final   = 70U;        // final scan voltage
    v.dV      = 1U;         // step size
    v.mock    = v.initial;  // mock voltage sent to dac

    // IV scan loop - queue mocks ready for calls by function under test
    while(v.mock <= v.final)
    {
        MocksForIvScanStep(mockLifeTester, &t, &v, TestGetAdcCodeConstantCurrent);
    }
    
    // Reset dac after measurements
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", initV)
        .withParameter("channel", mockLifeTester->io.dac);

    // Call function under test
    IV_ScanAndUpdate(mockLifeTester, v.initial, v.final, v.dV);

    // expect invalidScan error since the P(V) is linear not a hill.
    CHECK_EQUAL(invalidScan, mockLifeTester->error);
    // lifetester voltage should not have been been changed.
    CHECK_EQUAL(initV, mockLifeTester->data.vThis);
    
    // Check mock function calls match expectations
    mock().checkExpectations();
}

/*
 Test for running an IV scan where the measurement is interrupted at the critical
 point of measuring the mpp. No adc data is read from the device corresponding
 to that dac code. Point is reameasured and correct mpp is returned.
*/
TEST(IVTestGroup, RunIvScanInterruptedAtMppSample)
{
    const uint32_t initialTimestamp = 348775U; 
    TestTiming_t t = t;
    t.initial = initialTimestamp;
    t.mock = initialTimestamp;
    t.previous = initialTimestamp;
    t.dt = 100U;
    t.interrupt.requested = true;
    t.interrupt.start = SETTLE_TIME;
    t.interrupt.duration = SAMPLING_TIME;

    // IV scan setup - mock calls that happen prior to IV scan loop
    mock().expectOneCall("millis").andReturnValue(t.initial);
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    
    TestVoltage_t v = {
        .initial = 54U,
        .final = 62U,
        .dV = 1U,
        .mock = 54U
    };

    // flags
    bool dacSet = false;
    bool adcRead = false;

    // IV scan loop - queue mocks ready for calls by function under test
    while(v.mock <= v.final)
    {
        t.elapsed = t.mock - t.previous;
        InterruptScan(&v, &t);

        // Updating timer and flasher
        mock().expectOneCall("millis").andReturnValue(t.mock);
        mock().expectOneCall("Flasher::update");

        // (1) Set voltage during settle time
        if (t.elapsed < SETTLE_TIME)
        {
            mock().expectOneCall("DacSetOutput")
                .withParameter("output", v.mock)
                .withParameter("channel", mockLifeTester->io.dac);

            dacSet = true;
        }
        // (2) Sample current during measurement time window
        else if((t.elapsed >= SETTLE_TIME)
                && (t.elapsed < (SETTLE_TIME + SAMPLING_TIME)))
        {
            // Mock returns shockley diode current from lookup table
            mock().expectOneCall("AdcReadData")
                .withParameter("channel", mockLifeTester->io.adc)
                .andReturnValue(TestGetAdcCodeForDiode(v.mock));

            adcRead = true;
        }
        // (3) Do calculations - update timer for next measurement
        else
        {
            if (dacSet && adcRead)
            {
                v.mock += v.dV;
                dacSet = false;
                adcRead = false;
            }
            t.previous = t.mock;
            mock().expectOneCall("millis").andReturnValue(t.mock);
        }
        t.mock += t.dt;
    }
    
    // Reset dac after measurements
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", mppCodeShockley)
        .withParameter("channel", mockLifeTester->io.dac);

    // Call function under test
    IV_ScanAndUpdate(mockLifeTester, v.initial, v.final, v.dV);

    // Test assertions. Does the max power point agree with expectations
    // mpp dac code stored in lifetester instance
    const uint8_t mppCodeActual = mockLifeTester->data.vThis;
    CHECK_EQUAL(mppCodeShockley, mppCodeActual);
    // expect no errors
    CHECK_EQUAL(ok, mockLifeTester->error);
    
    // Check mock function calls match expectations
    mock().checkExpectations();
}

/*
 IV scan function should not be called if the lifetester object is in it's error
 state. The lifetester data should not change in this case.
*/
TEST(IVTestGroup, RunIvScanErrorStateDontScan)
{
    // already in error state. Function shouldn't run.
    mockLifeTester->error = currentLimit;

    // copy initial data and pass mock to function under test
    LifeTester_t lifeTesterExp = *mockLifeTester;

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
    lifeTesterExpected.data.iThis = iMock;
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
    lifeTesterExpected.data.iNext = iMock;
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
    const uint16_t iThis = MIN_CURRENT * nAdcSamples;     // ensure that the next point has more power
    const uint16_t iNext = (MIN_CURRENT + 20U) * nAdcSamples;

    const uint32_t pThis = iThis / nAdcSamples * vMock;
    const uint32_t pNext = iNext / nAdcSamples * (vMock + DV_MPPT);
    CHECK(pNext > pThis);

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->data.iThis = iThis;
    mockLifeTester->data.iNext = iNext;
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
    const uint16_t iThis = (MIN_CURRENT + 20U) * nAdcSamples;
    const uint16_t iNext = MIN_CURRENT * nAdcSamples;     // ensure that the next point has more power

    const uint32_t pCurrent = iThis / nAdcSamples * vMock;
    const uint32_t pNext = iNext / nAdcSamples * (vMock + DV_MPPT);
    CHECK(pNext < pCurrent);

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->data.iThis = iThis;
    mockLifeTester->data.iNext = iNext;
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
}