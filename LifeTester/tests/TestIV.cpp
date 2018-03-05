// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "IV.h"

// support
#include "Arduino.h"   // arduino function prototypes eg. millis (defined here)
#include "Config.h"
#include <stdio.h>     // REMOVE ME!

/*******************************************************************************
 * Private data and defines used for tests
 ******************************************************************************/
#define CURRENT_TO_CODE  (0.8)      // arbitrary scaling used to calculate adc code from current
#define FIXED_CURRENT    (2459U)    // fixed current output from bad device

enum lookupColumn {voltageData, currentData, powerData};

typedef struct TestTiming_s {
    uint32_t initial;  // initial time at beginning of call
    uint32_t mock;     // mock time returned from millis
    uint32_t previous; // previous measurement  
    uint32_t dt;       // time step in ms
} TestTiming_t;

// Note that voltages are sent as dac codes
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
    uint32_t tElapsed = t->mock - t->previous;
    if (tElapsed < SETTLE_TIME)
    {
        mock().expectOneCall("DacSetOutput")
            .withParameter("output", v->mock)
            .withParameter("channel", lifetester->channel.dac);
    }
    // (2) Sample current during measurement time window
    else if((tElapsed >= SETTLE_TIME) && (tElapsed < (SETTLE_TIME + SAMPLING_TIME)))
    {
        // Mock returns shockley diode current from lookup table
        uint8_t mockVolts = v->mock;
        mock().expectOneCall("AdcReadData")
            .withParameter("channel", lifetester->channel.adc)
            .andReturnValue(GetAdcCode(mockVolts));
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
// Define a test group for IV - all tests share common setup/teardown
TEST_GROUP(IVTestGroup)
{
    // Functions called after every test
    void setup(void)
    {

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
    // mock call to instantiate flasher object
    mock().expectOneCall("Flasher")
        .withParameter("pin", LED_A_PIN);

    // mock lifetester "object"
    LifeTester_t lifetester =
    {
        .channel = {chASelect, 0U},
        .Led = Flasher(LED_A_PIN)
        // Everything else default initialised to 0.
    };

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
        MocksForIvScanStep(&lifetester, &t, &v, TestGetAdcCodeForDiode);
    }
    
    // Reset dac after measurements
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", mppCodeShockley)
        .withParameter("channel", lifetester.channel.dac);

    // Call function under test
    IV_ScanAndUpdate(&lifetester, v.initial, v.final, v.dV);

    // Test assertions. Does the max power point agree with expectations
    // mpp dac code stored in lifetester instance
    const uint8_t mppCodeActual = lifetester.IVData.v;
    CHECK_EQUAL(mppCodeShockley, mppCodeActual);
    // expect no errors
    CHECK_EQUAL(ok, lifetester.error);
    CHECK_EQUAL(0, lifetester.nErrorReads);
    
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
    // mock call to instantiate flasher object
    mock().expectOneCall("Flasher")
        .withParameter("pin", LED_A_PIN);

    // mock lifetester "object"
    LifeTester_t lifetester =
    {
        .channel = {chASelect, 0U},
        .Led = Flasher(LED_A_PIN)
        // Everything else default initialised to 0.
    };

    const uint32_t tInitial = 348775U;
    uint32_t       mockTime = tInitial;
    uint32_t       tPrevious = mockTime;
    const uint32_t dt = 100U; // time step in ms

    // IV scan setup - mock calls that happen prior to IV scan loop
    mock().expectOneCall("millis").andReturnValue(tInitial);
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    
    // Note that voltages are sent as dac codes
    const uint8_t vInitial = 54U;
    const uint8_t vFinal   = 62U;
    const uint8_t dV       = 1U;
    uint16_t      mockVolts = vInitial;

    // flags
    bool dacSet = false;
    bool adcRead = false;
    bool interruptScan = true;

    // IV scan loop - queue mocks ready for calls by function under test
    while(mockVolts <= vFinal)
    {
        uint32_t tElapsed = mockTime - tPrevious;
        if ((mockVolts == mppCodeShockley)
            && (tElapsed >= SETTLE_TIME)
            && (tElapsed < (SETTLE_TIME + SAMPLING_TIME))
            && interruptScan)
        {
            mockTime += SAMPLING_TIME;
            interruptScan = false;
        }

        tElapsed = mockTime - tPrevious;
        // Updating timer and flasher
        mock().expectOneCall("millis").andReturnValue(mockTime);
        mock().expectOneCall("Flasher::update");

        // (1) Set voltage during settle time
        if (tElapsed < SETTLE_TIME)
        {
            mock().expectOneCall("DacSetOutput")
                .withParameter("output", mockVolts)
                .withParameter("channel", lifetester.channel.dac);

            dacSet = true;
        }
        // (2) Sample current during measurement time window
        else if((tElapsed >= SETTLE_TIME) && (tElapsed < (SETTLE_TIME + SAMPLING_TIME)))
        {
            // Mock returns shockley diode current from lookup table
            mock().expectOneCall("AdcReadData")
                .withParameter("channel", lifetester.channel.adc)
                .andReturnValue(TestGetAdcCodeForDiode(mockVolts));

            adcRead = true;
        }
        // (3) Do calculations - update timer for next measurement
        else
        {
            if (dacSet && adcRead)
            {
                mockVolts += dV;
                dacSet = false;
                adcRead = false;
            }
            tPrevious = mockTime;
            mock().expectOneCall("millis").andReturnValue(mockTime);
        }

        mockTime += dt;
    }
    
    // Reset dac after measurements
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", mppCodeShockley)
        .withParameter("channel", lifetester.channel.dac);

    // Call function under test
    IV_ScanAndUpdate(&lifetester, vInitial, vFinal, dV);

    // Test assertions. Does the max power point agree with expectations
    // mpp dac code stored in lifetester instance
    const uint8_t mppCodeActual = lifetester.IVData.v;
    CHECK_EQUAL(mppCodeShockley, mppCodeActual);
    // expect no errors
    CHECK_EQUAL(ok, lifetester.error);
    
    // Check mock function calls match expectations
    mock().checkExpectations();
}

/*
 IV scan function should not be called if the lifetester object is in it's error
 state. The lifetester data should not change in this case.
*/
TEST(IVTestGroup, RunIvScanErrorStateDontScan)
{
    // mock call to instantiate flasher object
    mock().expectOneCall("Flasher")
        .withParameter("pin", LED_A_PIN);

    const uint32_t initV = 11U;
    // mock lifetester "object" - set error state
    const LifeTester_t lifetesterInit =
    {
        {chASelect, 0U},
        Flasher(LED_A_PIN),
        0,
        0,
        0,
        lowCurrent,  // set error state
        {0},
        0
    };

    // copy initial data and pass to function under test
    LifeTester_t lifetesterFinal = lifetesterInit;

    // Note that voltages are sent as dac codes
    const uint8_t vInitial = 45U;
    const uint8_t vFinal   = 70U;
    const uint8_t dV       = 1U;

    // Call function under test
    IV_ScanAndUpdate(&lifetesterFinal, vInitial, vFinal, dV);
    // Check that the lifetester data hasn't changed
    MEMCMP_EQUAL(&lifetesterInit, &lifetesterFinal, sizeof(LifeTester_t));

    // No mocks should be called
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
    // mock call to instantiate flasher object
    mock().expectOneCall("Flasher")
        .withParameter("pin", LED_A_PIN);

    const uint32_t initV = 11U;
    // mock lifetester "object"
    LifeTester_t lifetester =
    {
        {chASelect, 0U},
        Flasher(LED_A_PIN),
        0,
        0,
        0,
        ok,
        {initV},
        0
    };

    const uint32_t tInitial = 348775U;
    uint32_t       mockTime = tInitial;
    uint32_t       tPrevious = mockTime;
    const uint32_t dt = 100U; // time step in ms

    // IV scan setup - mock calls that happen prior to IV scan loop
    mock().expectOneCall("millis").andReturnValue(tInitial);
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    
    // Note that voltages are sent as dac codes
    const uint8_t vInitial = 45U;
    const uint8_t vFinal   = 70U;
    const uint8_t dV       = 1U;
    uint16_t      mockVolts = vInitial;

    // IV scan loop - queue mocks ready for calls by function under test
    while(mockVolts <= vFinal)
    {
        // Setup
        mock().expectOneCall("millis").andReturnValue(mockTime);
        mock().expectOneCall("Flasher::update");

        // (1) Set voltage during settle time
        uint32_t tElapsed = mockTime - tPrevious;
        if (tElapsed < SETTLE_TIME)
        {
            mock().expectOneCall("DacSetOutput")
                .withParameter("output", mockVolts)
                .withParameter("channel", lifetester.channel.dac);
        }
        // (2) Sample current during measurement time window
        else if((tElapsed >= SETTLE_TIME) && (tElapsed < (SETTLE_TIME + SAMPLING_TIME)))
        {
            // Mock returns shockley diode current from lookup table
            mock().expectOneCall("AdcReadData")
                .withParameter("channel", lifetester.channel.adc)
                .andReturnValue(TestGetAdcCodeConstantCurrent(mockVolts));
        }
        // (3) Do calculations - update timer for next measurement
        else
        {
            mock().expectOneCall("millis").andReturnValue(mockTime);
            mockVolts += dV;
            tPrevious = mockTime;
        }

        mockTime += dt;
    }
    
    // Reset dac after measurements
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", initV)
        .withParameter("channel", lifetester.channel.dac);

    // Call function under test
    IV_ScanAndUpdate(&lifetester, vInitial, vFinal, dV);

    // expect invalidScan error since the P(V) is linear not a hill.
    CHECK_EQUAL(invalidScan, lifetester.error);
    // lifetester voltage should not have been been changed.
    CHECK_EQUAL(initV, lifetester.IVData.v);
    
    // Check mock function calls match expectations
    mock().checkExpectations();
}