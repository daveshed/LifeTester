// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "StateMachine.h"
#include "StateMachine_Private.h"

// support
#include "Arduino.h"   // arduino function prototypes eg. millis (defined here)
#include "Config.h"
#include "IoWrapper.h"
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
const static uint8_t mppCodeShockley = 58U;

// Mocks the time returned to the source by calls to millis
static uint32_t mockTime;

// Mocks the current returned from the adc
static uint16_t mockCurrent;

// Mock lifetester object that's used in lots of tests.
static LifeTester_t *mockLifeTester;

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

/*
 Mock implemetation for Arduino library function delay.
 */
void delay(unsigned long time)
{
    mock().actualCall("delay")
        .withParameter("time", time);
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

static bool ThisMeasurementActive(LifeTester_t const *const lifeTester)
{
    LifeTesterData_t const *const data = &lifeTester->data;
    return ((data->vActive == &data->vThis)
            && (data->iActive == &data->iThis)
            && (data->pActive == &data->pThis));
}

static bool NextMeasurementActive(LifeTester_t const *const lifeTester)
{
    LifeTesterData_t const *const data = &lifeTester->data;
    return ((data->vActive == &data->vNext)
            && (data->iActive == &data->iNext)
            && (data->pActive == &data->pNext));
}

static bool ScanMeasurementActive(LifeTester_t const *const lifeTester)
{
    LifeTesterData_t const *const data = &lifeTester->data;
    return ((data->vActive == &data->vScan)
            && (data->iActive == &data->iScan)
            && (data->pActive == &data->pScan));
}

static void MocksForInitDac(LifeTester_t const *const lifeTester)
{
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", 0U)
        .withParameter("channel", lifeTester->io.dac);
}
static void MocksForSetDacToScanVoltage(LifeTester_t const *const lifeTester)
{
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", lifeTester->data.vScan)
        .withParameter("channel", lifeTester->io.dac);
}

static void MocksForSetDacToThisVoltage(LifeTester_t const *const lifeTester)
{
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", lifeTester->data.vThis)
        .withParameter("channel", lifeTester->io.dac);
}

static void MocksForSetDacToNextVoltage(LifeTester_t const *const lifeTester)
{
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", lifeTester->data.vNext)
        .withParameter("channel", lifeTester->io.dac);
}

static void MocksForSetDacToVoltage(LifeTester_t const *const lifeTester,
                                    uint8_t v)
{
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", v)
        .withParameter("channel", lifeTester->io.dac);
}

static void MocksForSampleCurrent(LifeTester_t const *const lifeTester)
{
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", lifeTester->io.adc)
        .andReturnValue(mockCurrent);
}

static void MocksForGetTime(void)
{
    mock().expectOneCall("millis").
        andReturnValue(mockTime);
}

static void MocksForInitLedSetup(void)
{
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", INIT_LED_ON_TIME)
        .withParameter("offNew", INIT_LED_OFF_TIME);
    mock().expectOneCall("Flasher::keepFlashing");
}

static void MocksForScanLedSetup(void)
{
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    mock().expectOneCall("Flasher::keepFlashing");
}

static void MockForLedUpdate(void)
{
    mock().expectOneCall("Flasher::update");
}

static void MockForLedOff(void)
{
    mock().expectOneCall("Flasher::off");
}

static void MocksForMeasureScanPointEntry(LifeTester_t const *const lifeTester) 
{    
    MocksForSetDacToScanVoltage(mockLifeTester);
    MocksForGetTime();
}

static void MocksForScanModeEntry()
{
    MocksForScanLedSetup();
}

static void MocksForScanModeStep(void)
{
    MockForLedUpdate();
}

static void MocksForScanModeExit(void)
{
    MockForLedOff();
}

static void MocksForMeasureDataReadAdc(LifeTester_t const *const lifeTester)
{
    MocksForGetTime();
    MocksForSampleCurrent(mockLifeTester);
}

static void MocksForMeasureDataNoAdcRead(void)
{
    MocksForGetTime();
}

static void MocksForInitialiseStepAdcRead(LifeTester_t const *const lifeTester)
{
    MockForLedUpdate();
    MocksForGetTime();
    // delay time expired so adc will be sampled
    MocksForSampleCurrent(mockLifeTester);
}

static void MocksForInitialiseEntry(LifeTester_t const *const lifeTester)
{
    MocksForGetTime();
    MocksForInitDac(mockLifeTester);
    MocksForInitLedSetup();    
}

static void MocksForInitialiseStep(void)
{
    MocksForGetTime();
    MockForLedUpdate();
}

static void MocksForErrorLedSetup(void)
{
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", ERROR_LED_ON_TIME)
        .withParameter("offNew", ERROR_LED_OFF_TIME);
    mock().expectOneCall("Flasher::keepFlashing");
}

static void MocksForErrorEntry(LifeTester_t const *const lifeTester)
{
    MocksForErrorLedSetup();
    MocksForSetDacToVoltage(lifeTester, 0U);
}


static void SetupForScanningMode(LifeTester_t *const lifeTester)
{
    // note that mocks are needed to pass data to source
    const uint32_t tInit       = 2436U;
    const uint32_t tElapsed    = POST_DELAY_TIME + 1U;
                   mockTime    = tInit;
                   // Set current to get through init checks
                   mockCurrent = THRESHOLD_CURRENT + 1U; 
    MocksForInitialiseEntry(mockLifeTester);
    StateMachine_Reset(mockLifeTester);
    mockTime += tElapsed;
    MocksForInitialiseStepAdcRead(mockLifeTester);
    // mode change to Scanning mode - entry function sets led
    MocksForScanModeEntry();
    StateMachine_UpdateStep(mockLifeTester);
}

/*******************************************************************************
 * UNIT TESTS
 ******************************************************************************/
/*
 TODO: test required for timer overflow/wrap-around
*/
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
            0U,                 // timer
            ok,                 // error
            &StateNone
        };
        // Copy to a static variable for tests to work on
        static LifeTester_t lifeTesterForTest = lifetesterSetup;
        // Need to copy the data every time. A static is only initialised once.
        memcpy(&lifeTesterForTest, &lifetesterSetup, sizeof(LifeTester_t));
        // Access with a pointer from tests
        mockLifeTester = &lifeTesterForTest;
        mockTime = 0U;
        mockCurrent = 0U;
        mock().enable();
    }

    // Functions called after every test
    void teardown(void)
    {
        mock().clear();
    }
};

/*******************************************************************************
* TESTS FOR INITIALISATION
*******************************************************************************/
/*
 Update state machine while in initialise mode during post settle time. All data
 should be reset to 0 and the dac should be set to 0. No calls to read adc
 expected until settle time has elapsed.
*/
TEST(IVTestGroup, UpdatingInitialiseStateDuringPostDelayTime)
{
    // timer will be reset
    mockTime = 2436U;
    MocksForInitialiseEntry(mockLifeTester);
    StateMachine_Reset(mockLifeTester);
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    const uint8_t expectedData[sizeof(LifeTesterData_t)] = {0U};
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateInitialiseDevice, mockLifeTester->state);
    // timer will be checked again in step function.
    MocksForInitialiseStep();
    StateMachine_UpdateStep(mockLifeTester);
    // expect the state and data not to change
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateInitialiseDevice, mockLifeTester->state);
    CHECK_EQUAL(0U, DacGetOutput(mockLifeTester));
    mock().checkExpectations();
}

/*
 Update state machine while in initialise mode after post settle time. adc will 
 be read and value returned above threshold so no error expected. State machine
 will transition to Scanning mode.
*/
TEST(IVTestGroup, UpdatingInitialiseStateAfterPostDelayTimeAdcReadOk)
{
    const uint32_t tInit       = 2436U;
    const uint32_t tElapsed    = POST_DELAY_TIME + 1U;
                   mockTime    = tInit;
                   mockCurrent = THRESHOLD_CURRENT; 
    MocksForInitialiseEntry(mockLifeTester);
    StateMachine_Reset(mockLifeTester);
    CHECK_EQUAL(tInit, mockLifeTester->timer);
    const uint8_t expectedData[sizeof(LifeTesterData_t)] = {0U};
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateInitialiseDevice, mockLifeTester->state);

    // timer checked to see if measurement is due
    mockTime += tElapsed;
    MocksForInitialiseStepAdcRead(mockLifeTester);
    // Mode change to scanning expected - entry fn will setup led
    MocksForScanModeEntry();
    // New parent is NULL so nothing else should happen.
    StateMachine_UpdateStep(mockLifeTester);
    // expect the mode to change and for scan activated
    CHECK(ScanMeasurementActive(mockLifeTester));
    POINTERS_EQUAL(&StateScanningMode, mockLifeTester->state);
    CHECK_EQUAL(ok, mockLifeTester->error);
    mock().checkExpectations();
}

/*
 Update state machine while in initialise mode after post settle time. adc will 
 be read but the value returned is below threshold so error and state change
 expected.
*/
TEST(IVTestGroup, UpdatingInitialiseStateAfterPostDelayTimeAdcReadLowCurrent)
{
    const uint32_t tInit       = 2436U;
    const uint32_t tElapsed    = POST_DELAY_TIME + 1U;
                   mockTime    = tInit;
                   mockCurrent = THRESHOLD_CURRENT - 1U; 
    MocksForInitialiseEntry(mockLifeTester);
    StateMachine_Reset(mockLifeTester);
    CHECK_EQUAL(tInit, mockLifeTester->timer);
    const uint8_t expectedData[sizeof(LifeTesterData_t)] = {0U};
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateInitialiseDevice, mockLifeTester->state);
    // timer will be checked again in step function.
    mockTime += tElapsed;
    MocksForInitialiseStepAdcRead(mockLifeTester);
    MocksForErrorEntry(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    // expect the mode to change but not data
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateError, mockLifeTester->state);
    CHECK_EQUAL(currentThreshold, mockLifeTester->error);
    mock().checkExpectations();
}

/*
 Update state machine while in initialise mode after post settle time. adc will 
 be read but the value returned is saturated so error and state change expected.
*/
TEST(IVTestGroup, UpdatingInitialiseStateAfterPostDelayTimeAdcReadSaturated)
{
    const uint32_t tInit       = 2436U;
    const uint32_t tElapsed    = POST_DELAY_TIME + 1U;
                   mockTime    = tInit;
                   mockCurrent = MAX_CURRENT; 
    MocksForInitialiseEntry(mockLifeTester);
    StateMachine_Reset(mockLifeTester);
    CHECK_EQUAL(tInit, mockLifeTester->timer);
    const uint8_t expectedData[sizeof(LifeTesterData_t)] = {0U};
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateInitialiseDevice, mockLifeTester->state);
    // timer will be checked again in step function.
    mockTime += tElapsed;
    MocksForInitialiseStepAdcRead(mockLifeTester);
    MocksForErrorEntry(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    // expect the mode to change but not data
    MEMCMP_EQUAL(expectedData, &mockLifeTester->data, sizeof(LifeTesterData_t));
    POINTERS_EQUAL(&StateError, mockLifeTester->state);
    CHECK_EQUAL(currentLimit, mockLifeTester->error);
    mock().checkExpectations();
}

/*TODO: test for dac not set - mocks don't allow this at present. Need to signal
that the dac is broken so that setting does not actually change the dac output.*/

/*******************************************************************************
* TESTS FOR UPDATING IN SCANNING MODE
********************************************************************************/
/*
 Test that updating the state machine while in scanning mode does nothing when
 no adc reading is due (during settle time).
*/
TEST(IVTestGroup, UpdatingInScanModeBeforeSamplingWindowDoesNothing)
{
    SetupForScanningMode(mockLifeTester);
    const LifeTesterData_t dataBefore = mockLifeTester->data;
    // mock for scanning mode step
    MocksForScanModeStep();
    MocksForMeasureScanPointEntry(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    POINTERS_EQUAL(&StateMeasureScanDataPoint, mockLifeTester->state);
    // mocks for update during sampling before settled
    mockTime += SETTLE_TIME - 1U;
    MocksForScanModeStep();
    MocksForMeasureDataNoAdcRead();
    StateMachine_UpdateStep(mockLifeTester);
    const LifeTesterData_t dataAfter = mockLifeTester->data;
    // data shouldn't have changed.
    MEMCMP_EQUAL(&dataBefore, &dataAfter, sizeof(LifeTesterData_t));    
    mock().checkExpectations();
}

/*
 Test that updating the state machine while in scanning measurement mode takes
 an adc sample provided that time is within sample time. Several measurements
 are taken. Expect an average.
*/
TEST(IVTestGroup, UpdatingScanInSampleScanCurrentAdcMeasurementExpected)
{
    SetupForScanningMode(mockLifeTester);
    // Now in scanning mode parent of measure scan point
    const uint8_t vMock = 32U;
    mockLifeTester->data.vScan = vMock; 
    MocksForScanModeStep();
    MocksForMeasureScanPointEntry(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    mockTime += SETTLE_TIME;

    // queue up some current measurements
    const uint32_t iMock[]       = {3487U, 2435U, 3488U};
    const uint32_t nMeasurements = 3U;
    for (int i = 0; i < nMeasurements; i++)
    {
        mockTime += 10U;
        const uint32_t tElapsed = mockTime - mockLifeTester->timer;
        const bool     sampling = (tElapsed >= SETTLE_TIME)
                                   && (tElapsed < (SETTLE_TIME + SAMPLING_TIME));
        CHECK(sampling);
        MocksForScanModeStep();
        mockCurrent = iMock[i];
        MocksForMeasureDataReadAdc(mockLifeTester);
        StateMachine_UpdateStep(mockLifeTester);
    }
    const uint32_t iMockSum = (iMock[0] + iMock[1] + iMock[2]);
    CHECK_EQUAL(iMockSum, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(nMeasurements, mockLifeTester->data.nSamples);
    POINTERS_EQUAL(&StateMeasureScanDataPoint, mockLifeTester->state);
    // sampling finished. Check average is calculated.
    mockTime += SAMPLING_TIME;
    MocksForScanModeStep();
    MocksForMeasureDataNoAdcRead();
    StateMachine_UpdateStep(mockLifeTester); 
    const uint32_t iMockAve = iMockSum / nMeasurements;
    CHECK_EQUAL(iMockAve, mockLifeTester->data.iScan);
    const uint32_t pExpected = vMock * iMockAve;
    CHECK_EQUAL(pExpected, mockLifeTester->data.pScan);
    // expect to have exited from nested scan measure data point to parent
    POINTERS_EQUAL(&StateScanningMode, mockLifeTester->state)
    CHECK_EQUAL(vMock + DV_SCAN, mockLifeTester->data.vScan);
    mock().checkExpectations();
}

/*
 If no samples are taken in measure scan data point during the sampling window
 then the timer should be reset so the sampling window can run again. No 
 transition is expected.
*/
TEST(IVTestGroup, UpdatingScanNoAdcSamplesTimerShouldReset)
{
    SetupForScanningMode(mockLifeTester);
    // mock for scanning mode step
    MocksForScanModeStep();
    // mocks for measaure scan point entry
    MocksForMeasureScanPointEntry(mockLifeTester);
    // transition into measure scan point fn
    StateMachine_UpdateStep(mockLifeTester); 
    POINTERS_EQUAL(&StateMeasureScanDataPoint, mockLifeTester->state);
    mockTime += (SETTLE_TIME + SAMPLING_TIME);
    MocksForScanModeStep();
    MocksForMeasureDataNoAdcRead();
    StateMachine_UpdateStep(mockLifeTester); 
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    mock().checkExpectations();
}

/*
 Test for running through an iv scan without errors. Expect to return the mpp 
 of a shockley diode. Start in the initialise state. System goes through POST
 and then on to scan mode.
*/
TEST(IVTestGroup, RunIvScanNoErrorExpectDiodeMppReturned)
{    
    SetupForScanningMode(mockLifeTester);
    uint8_t vMock = V_SCAN_MIN; 
    CHECK_EQUAL(vMock, mockLifeTester->data.vScan);    
    POINTERS_EQUAL(&StateScanningMode, mockLifeTester->state);
    // keep executing scan loop until finished.
    while (vMock <= V_SCAN_MAX)
    {
        // printf("v %u v_actual %u t %u mode %s\n",
            // vMock, mockLifeTester->data.vScan, mockTime, mockLifeTester->state->label);
        // Transition into measure scan point...
        MocksForScanModeStep();
        // mocks for measaure scan point entry
        MocksForMeasureScanPointEntry(mockLifeTester);
        StateMachine_UpdateStep(mockLifeTester);
        // Force a measurement
        mockTime += SETTLE_TIME;
        mockCurrent = TestGetAdcCodeForDiode(vMock);
        MocksForScanModeStep();
        MocksForMeasureDataReadAdc(mockLifeTester);
        StateMachine_UpdateStep(mockLifeTester);
        POINTERS_EQUAL(&StateMeasureScanDataPoint, mockLifeTester->state);
        CHECK_EQUAL(vMock, DacGetOutput(mockLifeTester));
        // printf("v %u v_actual %u t %u mode %s\n",
            // vMock, mockLifeTester->data.vScan, mockTime, mockLifeTester->state->label);
        // sampling done. Transition back to scanning mode (parent)
        mockTime += SAMPLING_TIME;
        vMock += DV_SCAN;
        MocksForScanModeStep();
        MocksForMeasureDataNoAdcRead();
        StateMachine_UpdateStep(mockLifeTester);
    }
    printf("mode %s\n", mockLifeTester->state->label);
    POINTERS_EQUAL(&StateScanningMode, mockLifeTester->state);
    // should change from scanning->tracking mode
    MocksForScanModeStep();
    MocksForScanModeExit();
    StateMachine_UpdateStep(mockLifeTester);
    CHECK_EQUAL(mppCodeShockley, mockLifeTester->data.vThis);
    mock().checkExpectations();
}

/*
 Test for running through an iv scan without errors. Mock device connected is an
 open circuit - power shape is not hill shaped. Expect error to be returned.
*/
TEST(IVTestGroup, RunIvScanBadDiodeNoMppReturned)
{    
    SetupForScanningMode(mockLifeTester);
    uint8_t vMock = V_SCAN_MIN; 
    CHECK_EQUAL(vMock, mockLifeTester->data.vScan);    
    POINTERS_EQUAL(&StateScanningMode, mockLifeTester->state);
    // keep executing scan loop until finished.
    while (vMock <= V_SCAN_MAX)
    {
        // Transition into measure scan point...
        MocksForScanModeStep();
        // mocks for measaure scan point entry
        MocksForMeasureScanPointEntry(mockLifeTester);
        StateMachine_UpdateStep(mockLifeTester);
        // Force a measurement
        mockTime += SETTLE_TIME;
        mockCurrent = TestGetAdcCodeConstantCurrent(vMock);
        MocksForScanModeStep();
        MocksForMeasureDataReadAdc(mockLifeTester);
        StateMachine_UpdateStep(mockLifeTester);
        POINTERS_EQUAL(&StateMeasureScanDataPoint, mockLifeTester->state);
        CHECK_EQUAL(vMock, DacGetOutput(mockLifeTester));
        // sampling done. Transition back to scanning mode (parent)
        mockTime += SAMPLING_TIME;
        vMock += DV_SCAN;
        MocksForScanModeStep();
        MocksForMeasureDataNoAdcRead();
        StateMachine_UpdateStep(mockLifeTester);
    }
    POINTERS_EQUAL(&StateScanningMode, mockLifeTester->state);
    // should change from scanning->error mode
    MocksForScanModeStep();
    MocksForScanModeExit();
    MocksForErrorEntry(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    CHECK_EQUAL(0U, mockLifeTester->data.vThis);
    CHECK_EQUAL(invalidScan, mockLifeTester->error);
    POINTERS_EQUAL(&StateError, mockLifeTester->state);
    mock().checkExpectations();
}
#if 0

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
    mockLifeTester->state = StateWaitForTrackingDelay;

    // Check time. millis should return time within tracking delay time window
    mock().expectOneCall("millis").andReturnValue(tMock);
    // Call function under test
    const LifeTester_t lifeTesterExp = *mockLifeTester;
    StateMachine_Update(mockLifeTester);
    const LifeTester_t lifeTesterActual = *mockLifeTester;
    // Expect no change to lifetester data
    MEMCMP_EQUAL(&lifeTesterExp, &lifeTesterActual, sizeof(LifeTester_t));
    mock().checkExpectations();
}

/*
 Test for updating mpp during settle time. Expect the dac to be set only. No
 calls to read adc and no change to lifetester data.
*/
TEST(IVTestGroup, UpdateMppDuringSettleTimeForThisPoint)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME + 10U; // time elapsed since
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 43U;

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSetToThisVoltage;

    // Dac is not set
    CHECK(!DacOutputSetToThisVoltage(mockLifeTester));

    MocksForSetDacToThisVoltage(mockLifeTester);
    StateMachine_Update(mockLifeTester);
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    POINTERS_EQUAL(StateSampleThisCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*******************************************************************************
 TESTS FOR GENERIC MEASURE DATA POINT (ENTRY, EXIT, STEP FUNCTIONS)
*******************************************************************************/
/*
 Test for making a transiton to the StateMeasureThisPoint state from a null state.
 The dac should be set, timer and current sampling variables should be reset.
*/
TEST(IVTestGroup, StateTransitionToMeasureThisDataPointCallsDacSet)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
                   mockTime  = 329683U;
    const uint32_t vMock     = 47U;                   // voltage at current op point

    // ensure that the dac isn't set before beginning
    mock().disable();
    DacSetOutput(0U, mockLifeTester->io.dac);
    mock().enable();

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->currentState = StateNone;

    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock)
        .withParameter("channel", mockLifeTester->io.dac);
    mock().expectOneCall("millis").andReturnValue(mockTime); 

    // Call function under test
    StateMachineTransitionToState(mockLifeTester, StateMeasureThisDataPoint);
    // Current sampling variables should be reset.
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    // State transition should have taken place
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint, 
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
}

/*
 Test for making a transiton to the StateMeasureThisPoint state from a null state.
 Dac is already set so no call to dac set output is expected.
*/
TEST(IVTestGroup, StateTransitionToMeasureThisDataPointDacAlreadySet)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
                   mockTime  = 329683U;
    const uint32_t vMock     = 47U;                   // voltage at current op point

    // ensure that the dac is already set before beginning
    mock().disable();
    DacSetOutput(vMock, mockLifeTester->io.dac);
    mock().enable();
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->currentState = StateNone;

    mock().expectOneCall("millis").andReturnValue(mockTime); 
    // Call function under test
    StateMachineTransitionToState(mockLifeTester, StateMeasureThisDataPoint);
    // Current sampling variables should be reset.
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    // State transition should have taken place
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint, 
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
}

/*
 Test for updating the state machine while already in StateMeasureThisDataPoint.
 Expect the step function to be called and for the dac to be reset via the state
 entry function. Should end up back in the same state.
*/
TEST(IVTestGroup, UpdateInStateMeasureThisDataPointDacNotSet)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
                   mockTime  = 329683U;
    const uint32_t vMock     = 47U;                   // voltage at current op point

    // ensure that the dac is not set before beginning
    mock().disable();
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->currentState = StateNone;
    StateMachineTransitionToState(mockLifeTester, StateMeasureThisDataPoint);
    DacSetOutput(0U, mockLifeTester->io.dac);
    mock().enable();
    // first timer is checked against millis
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // step function notices that dac isn't set correctly then sets it
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock)
        .withParameter("channel", mockLifeTester->io.dac);
    // millis read again by the entry function as we exit then reenter state
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // call function under test.
    StateMachine_UpdateStep(mockLifeTester);
    // timer is reset for the measurement to restart
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    // Should reenter the same state.
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint, 
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
}

/*
 Test for updating the state machine while already in StateMeasureThisDataPoint.
 Dac is already set but we have updated before sampling is due (during settle
 time). So nothing should happen.
*/
TEST(IVTestGroup, UpdateInStateMeasureThisDataPointDacSetDuringSettleTime)
{
    const uint32_t tPrevious = 239348U;
    const uint32_t tElapsed =  SETTLE_TIME - 1U;      // set millis to return within settle time
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock     = 47U;                   // voltage at current op point

    mock().disable();
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->currentState = StateNone;
    // transition function should set the dac
    StateMachineTransitionToState(mockLifeTester, StateMeasureThisDataPoint);
    CHECK_EQUAL(vMock, DacGetOutput(mockLifeTester));
    mockLifeTester->timer = tPrevious;
    mock().enable();
    // timer should be checked against millis
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // call function under test.
    StateMachine_UpdateStep(mockLifeTester);
    // timer should not be reset. Waiting for sampling to start
    CHECK_EQUAL(tPrevious, mockLifeTester->timer);
    // Should reenter the same state.
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint, 
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
}

/*
 Test for updating the state machine while already in StateMeasureThisDataPoint.
 Dac is already set, samples are taken during the sampling window and when it
 finishes, the mode should change. Current will be averaged.
*/
TEST(IVTestGroup, UpdateInStateMeasureThisDataPointDacSetSamplingDone)
{
    const uint32_t tPrevious = 239348U;
                   mockTime = tPrevious; // value to return from millis
    const uint32_t vMock     = 47U;      // voltage at current op point
    const uint16_t mockCurrent1 = 45829U;
    const uint16_t mockCurrent2 = 46836U;
    const uint16_t nSamples = 2U;
    const uint16_t iExpected = (mockCurrent1 + mockCurrent2) / nSamples;

    mock().disable();
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->currentState = StateNone;
    // Force a transition into required state to set stuff
    StateMachineTransitionToState(mockLifeTester, StateMeasureThisDataPoint);
    // Ensure that the dac is set as specified by test.
    CHECK_EQUAL(vMock, DacGetOutput(mockLifeTester));
    mockLifeTester->timer = tPrevious;
    mock().enable();
    // call function under test during sampling window.
    mockTime += SETTLE_TIME + 10U;
    mock().expectOneCall("millis").andReturnValue(mockTime);
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", mockLifeTester->io.adc)
        .andReturnValue(mockCurrent1);
    StateMachine_UpdateStep(mockLifeTester);
    // Call again, so another sample should be taken
    mock().expectOneCall("millis").andReturnValue(mockTime);
    mock().expectOneCall("AdcReadData")
        .withParameter("channel", mockLifeTester->io.adc)
        .andReturnValue(mockCurrent2);
    StateMachine_UpdateStep(mockLifeTester);
    // Expect two measurements to be taken and for mode to be the same
    CHECK_EQUAL((mockCurrent1 + mockCurrent2), mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(nSamples, mockLifeTester->data.nSamples);
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint,
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    // increment timer outside the sampling window - expect mode change
    mockTime += SAMPLING_TIME;
    mock().expectOneCall("millis").andReturnValue(mockTime);
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock + DV_MPPT)
        .withParameter("channel", mockLifeTester->io.dac);
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_UpdateStep(mockLifeTester);
    CHECK_EQUAL(iExpected, mockLifeTester->data.iThis);
    MEMCMP_EQUAL(
        &StateMeasureNextDataPoint,
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
}

/*******************************************************************************
* Tests for update state machine in sampling time for THIS point
*******************************************************************************/
/*
 Test for updating mpp during sampling time window. Expect the adc to be read and
 relevant data in the lifetester object to be updated.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForThisPoint)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed =  SETTLE_TIME + 10U;     // time elapsed since
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleThisCurrent;

    /*Need to set the mock dac hardware to this voltage otherwise the lifetester
    won't actually sample any current. Measurement will be reset. Do this 
    outside of mocking because this is setup and not actual behaviour of function.*/
    mock().disable();
    DacSetOutputToThisVoltage(mockLifeTester);
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    mock().enable();

    // Expect call to adc since we're in the current sampling window (millis also)
    mockCurrent = TestGetAdcCodeForDiode(vMock);
    MocksForSampleCurrent(mockLifeTester);
    // Call function under test
    StateMachine_Update(mockLifeTester);

    // expect the current and number of readings to be updated only
    CHECK_EQUAL(mockCurrent, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(1U, mockLifeTester->data.nSamples);
    mock().checkExpectations();
}

/*
 Test for updating mpp before during sampling time window. Timer should be checked
 The adc should not be read and state will stay the same ready for the next update.
*/
TEST(IVTestGroup, UpdateMppInSampleThisCurrentStateBeforeSampleTime)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = SETTLE_TIME - 10U;      // time elapsed since
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleThisCurrent;
    /*Ensure that the dac is set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutputToThisVoltage(mockLifeTester);
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    mock().enable();
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iThis);
    
    /* Expect a call to millis. Function will check whether a measurement is due.
    Do not expect any calls to read the adc. */
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    
    // Measurements shouldn't change.
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iThis);
    // expect the timer and mode to stay the same
    CHECK_EQUAL(tPrevious, mockLifeTester->timer);
    POINTERS_EQUAL(StateSampleThisCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*
 Test for updating mpp after sampling time window. Adc hasn't been sampled but
 the dac has been set. Expect timer to be reset and to stay in sample this
 current mode.
*/
TEST(IVTestGroup, UpdateMppInSampleThisCurrentAfterSampleWindowAdcNotRead)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = SETTLE_TIME + SAMPLING_TIME + 10U;
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleThisCurrent;
    /*Ensure that the dac is set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutputToThisVoltage(mockLifeTester);
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    mock().enable();
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iThis);
    
    /* Expect a call to millis. Function will check whether a measurement is due.
    Do not expect any calls to read the adc. */
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    
    // Measurements shouldn't change.
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iThis);
    // expect the timer to be reset and mode to stay the same
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    POINTERS_EQUAL(StateSampleThisCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*
 Test for updating mpp before sampling time window. The adc should not be read.
 Expect millis to be called and for mode to stay the same.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForThisPointDacNotSet)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = SETTLE_TIME + SAMPLING_TIME - 10U;
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleThisCurrent;

    /*Ensure that the dac is not set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutput(vMock - 10U, mockLifeTester->io.dac);
    CHECK(!DacOutputSetToThisVoltage(mockLifeTester));
    mock().enable();

    // Check time - timer should be restarted. Expect call to millis.
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // Call function under test
    StateMachine_Update(mockLifeTester);

    // expect the timer to be updated and mode to change
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    POINTERS_EQUAL(StateSampleThisCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*******************************************************************************
* Tests for update state machine in sampling time for NEXT point
*******************************************************************************/
/*
 Test for updating mpp during sampling time window. Expect the adc to be read and
 relevant data in the lifetester object to be updated.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForNextPoint)
{

    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed =  SETTLE_TIME + 10U;     // time elapsed since
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    mockLifeTester->data.vNext = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleNextCurrent;

    /*Need to set the mock dac hardware to Next voltage otherwise the lifetester
    won't actually sample any current. Measurement will be reset. Do this 
    outside of mocking because this is setup and not actual behaviour of function.*/
    mock().disable();
    DacSetOutputToNextVoltage(mockLifeTester);
    CHECK(DacOutputSetToNextVoltage(mockLifeTester));
    mock().enable();

    // Expect call to adc since we're in the current sampling window (millis also)
    mockCurrent = TestGetAdcCodeForDiode(vMock);
    MocksForSampleCurrent(mockLifeTester);
    // Call function under test
    StateMachine_Update(mockLifeTester);

    // expect the current and number of readings to be updated only
    CHECK_EQUAL(mockCurrent, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(1U, mockLifeTester->data.nSamples);
    mock().checkExpectations();
}

/*
 Test for updating mpp before during sampling time window. Timer should be checked
 The adc should not be read and state will stay the same ready for the next update.
*/
TEST(IVTestGroup, UpdateMppInSampleNextCurrentStateBeforeSampleTime)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = SETTLE_TIME - 10U;      // time elapsed since
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    mockLifeTester->data.vNext = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleNextCurrent;
    /*Ensure that the dac is set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutputToNextVoltage(mockLifeTester);
    CHECK(DacOutputSetToNextVoltage(mockLifeTester));
    mock().enable();
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iNext);
    
    /* Expect a call to millis. Function will check whether a measurement is due.
    Do not expect any calls to read the adc. */
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    
    // Measurements shouldn't change.
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iNext);
    // expect the timer and mode to stay the same
    CHECK_EQUAL(tPrevious, mockLifeTester->timer);
    POINTERS_EQUAL(StateSampleNextCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*
 Test for updating mpp after sampling time window. Adc hasn't been sampled but
 the dac has been set. Expect timer to be reset and to stay in sample this
 current mode.
*/
TEST(IVTestGroup, UpdateMppInSampleNextCurrentAfterSampleWindowAdcNotRead)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = SETTLE_TIME + SAMPLING_TIME + 10U;
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    mockLifeTester->data.vNext = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleNextCurrent;
    /*Ensure that the dac is set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutputToNextVoltage(mockLifeTester);
    CHECK(DacOutputSetToNextVoltage(mockLifeTester));
    mock().enable();
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iNext);
    
    /* Expect a call to millis. Function will check whether a measurement is due.
    Do not expect any calls to read the adc. */
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    
    // Measurements shouldn't change.
    CHECK_EQUAL(0U, mockLifeTester->data.nSamples);
    CHECK_EQUAL(0U, mockLifeTester->data.iSampleSum);
    CHECK_EQUAL(0U, mockLifeTester->data.iNext);
    // expect the timer to be reset and mode to stay the same
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    POINTERS_EQUAL(StateSampleNextCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*
 Test for updating mpp before sampling time window. The adc should not be read.
 Expect millis to be called and for mode to stay the same.
*/
TEST(IVTestGroup, UpdateMppDuringSamplingTimeForNextPointDacNotSet)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = SETTLE_TIME + SAMPLING_TIME - 10U;
                   mockTime = tPrevious + tElapsed;   // value to return from millis
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    mockLifeTester->data.vNext = vMock;
    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateSampleNextCurrent;

    /*Ensure that the dac is not set before updating. The state machine should 
    realise this and change mode accordingly.*/
    mock().disable();
    DacSetOutput(vMock - 10U, mockLifeTester->io.dac);
    CHECK(!DacOutputSetToNextVoltage(mockLifeTester));
    mock().enable();

    // Check time - timer should be restarted. Expect call to millis.
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // Call function under test
    StateMachine_Update(mockLifeTester);

    // expect the timer to be updated and mode to change
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    POINTERS_EQUAL(StateSampleNextCurrent, mockLifeTester->state);
    mock().checkExpectations();
}

/*
 Test for updating mpp after all measurements complete. Data stored in lifetester
 object. Expect function to calculate which point provides the greatest power
 and set the stored value of voltage accordingly.
*/
TEST(IVTestGroup, UpdateMppAfterMeasurementsExpectIncreaseVoltage)
{
    const uint32_t tPrevious = 32456U;
    const uint32_t tElapsed = SETTLE_TIME + SAMPLING_TIME + 1U;
                   mockTime = tPrevious + tElapsed;  // sampling done.
    const uint32_t vThis = 47U;                      // vThis
    const uint32_t vNext = 47U + DV_MPPT;
    
    mockCurrent = 2324U;

    // const uint16_t iThis = MIN_CURRENT;     
    // const uint16_t iNext = iThis + 20U; // ensure that next point has more power

    // const uint32_t pThis = iThis * vMock;
    // const uint32_t pNext = iNext * (vMock + DV_MPPT);

    mockLifeTester->timer = tPrevious;
    mockLifeTester->data.vThis = vThis;
    mockLifeTester->data.vNext = vNext; // shouldn't need this. Entry function sets it
    mockLifeTester->currentState = StateNone;

    // 1. Start by measuring this point - expect dac to be set and timer reset
    MocksForGetTime();
    MocksForSetDacToThisVoltage(mockLifeTester);
    StateMachineTransitionToState(mockLifeTester, StateMeasureThisDataPoint);
    // 2. Take a measurement
    mockTime += SETTLE_TIME + 1U;
    MocksForGetTime();
    MocksForSampleCurrent(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint,
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
    // 3. Transition to next state
    mockTime += SAMPLING_TIME;
    MocksForGetTime();
    MocksForSetDacToNextVoltage(mockLifeTester);
    MocksForGetTime();  // called again to reset timer
    StateMachine_UpdateStep(mockLifeTester);
    // 4. Take a measurement
    mockTime += SETTLE_TIME + 1U;
    MocksForGetTime();
    MocksForSampleCurrent(mockLifeTester);
    StateMachine_UpdateStep(mockLifeTester);
    // 5. call analyse measurement exit function
    mockTime += SAMPLING_TIME;
    MocksForGetTime();
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 2);
    MocksForGetTime();
    MocksForSetDacToThisVoltage(mockLifeTester);
    MocksForGetTime(); // timer reset after setting dac
    StateMachine_UpdateStep(mockLifeTester);
    // timer will be checked in step function
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // Expect call to change led state - indicates increase in voltage
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 2);
    // Expect another call to millis to update the value of the timmer in exit fn
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // Expect dac to be set to the next voltage entry to measure this.
    mock().expectOneCall("DacSetOutput")
        .withParameter("output", vMock + DV_MPPT)
        .withParameter("channel", mockLifeTester->io.dac);
    
    // Call function under test
    StateMachine_UpdateStep(mockLifeTester);

    // Expect that working voltage should increase
    CHECK_EQUAL(vMock + DV_MPPT, mockLifeTester->data.vThis);
    // that timer should be updated since measurement is done
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    // check there's no error
    CHECK_EQUAL(ok, mockLifeTester->error);
    // State should change now that measurements are done
    MEMCMP_EQUAL(
        &StateMeasureThisDataPoint,
        &mockLifeTester->currentState,
        sizeof(LifeTesterState_t));
    mock().checkExpectations();
}

/*
 Test for updating mpp after all measurements complete. Data stored in lifetester
 object. Expect function to determine that the current point has more power so
 it should decide to decrement the working voltage. 
*/
TEST(IVTestGroup, UpdateMppAfterMeasurementsDecreaseVoltage)
{
    mockTime = 62432U;
    const uint32_t vMock = 47U;                       // voltage at current op point
    
    const uint16_t nAdcSamples = 4U;
    const uint16_t iNext = MIN_CURRENT;     
    const uint16_t iThis = iNext + 20U; // ensure that this point has more power

    const uint32_t pThis = iThis * vMock;
    const uint32_t pNext = iNext * (vMock + DV_MPPT);

    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;
    mockLifeTester->data.iThis = iThis;
    mockLifeTester->data.iNext = iNext;
    mockLifeTester->data.pThis = pThis;
    mockLifeTester->data.pNext = pNext;
    mockLifeTester->data.nSamples = nAdcSamples;
    mockLifeTester->state = StateAnalyseTrackingMeasurement;

    CHECK(mockLifeTester->data.pNext < mockLifeTester->data.pThis);
    // Expect call to change led state - indicates decrease in voltage
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 1);
    // Expect another call to millis to update the value of the timmer in lifetester object
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // Call function under test
    StateMachine_Update(mockLifeTester);

    // Exoect that working voltage should reduce
    CHECK_EQUAL(vMock - DV_MPPT, mockLifeTester->data.vThis);
    // that timer should be updated since measurement is done
    CHECK_EQUAL(mockTime, mockLifeTester->timer);
    // expect low current error here since iNext and iCurrent are very low
    CHECK_EQUAL(ok, mockLifeTester->error);
    // State should change now that measurements are done
    CHECK_EQUAL(StateWaitForTrackingDelay, mockLifeTester->state);
    mock().checkExpectations();
}

/*
 Test for state transitions from initial tracking delay to set voltage and then
 to sample current etc for this and next measurement.
*/
TEST(IVTestGroup, IndividualStateTransitionsFromTrackingDelayOnwards)
{
    const uint32_t tPrevious = 239348U;               // last time mpp was updated 
    const uint32_t tElapsed = TRACK_DELAY_TIME + 1U;  // set elapsed time in settle time
                   mockTime = tPrevious + tElapsed;      // value to return from millis

    const uint32_t vMock = 4U;

    mockLifeTester->timer = tPrevious;
    mockLifeTester->state = StateWaitForTrackingDelay;
    mockLifeTester->data.vThis = vMock;
    mockLifeTester->data.vNext = vMock + DV_MPPT;

    // Time checked by update function
    mock().expectOneCall("millis").andReturnValue(mockTime);
    // Call function under test
    StateMachine_Update(mockLifeTester);
    // Expect change in state because time exceeds tracking delay
    POINTERS_EQUAL(StateSetToThisVoltage, mockLifeTester->state);
    /*Update again. Setting voltage leads to a transition. Dac only needs to 
    be set once before transition to sampling mode*/
    CHECK(!DacOutputSetToThisVoltage(mockLifeTester));
    CHECK_EQUAL(ok, mockLifeTester->error);
    MocksForSetDacToThisVoltage(mockLifeTester);
    StateMachine_Update(mockLifeTester);
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    POINTERS_EQUAL(StateSampleThisCurrent, mockLifeTester->state);
    /*Update again in the sampling window. Expect current to be measured but no
    transition should happen*/
    mockTime += SETTLE_TIME;
    CHECK(DacOutputSetToThisVoltage(mockLifeTester));
    mockCurrent = 3245U;
    CHECK_EQUAL(ok, mockLifeTester->error);
    MocksForSampleCurrent(mockLifeTester);
    StateMachine_Update(mockLifeTester);
    POINTERS_EQUAL(StateSampleThisCurrent, mockLifeTester->state);

    /* Increment time into the second settle time. Expect transition. */
    mockTime += SAMPLING_TIME;
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    POINTERS_EQUAL(StateSetToNextVoltage, mockLifeTester->state);

    /* Expect the voltage to be set to the next point amd for state to change */
    CHECK(!DacOutputSetToNextVoltage(mockLifeTester));
    CHECK_EQUAL(ok, mockLifeTester->error);
    MocksForSetDacToNextVoltage(mockLifeTester);
    StateMachine_Update(mockLifeTester);
    CHECK(DacOutputSetToNextVoltage(mockLifeTester));
    POINTERS_EQUAL(StateSampleNextCurrent, mockLifeTester->state);

    /* Increment timer again into the next sampling window. */
    mockTime += SETTLE_TIME;
    MocksForSampleCurrent(mockLifeTester);
    StateMachine_Update(mockLifeTester);
    CHECK_EQUAL(ok, mockLifeTester->error);
    POINTERS_EQUAL(StateSampleNextCurrent, mockLifeTester->state);

    /* Sampling done. Expect transition to measurement analysis */
    mockTime += SAMPLING_TIME;
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    CHECK_EQUAL(ok, mockLifeTester->error);
    POINTERS_EQUAL(StateAnalyseTrackingMeasurement, mockLifeTester->state);

    /* Updating again should restart the whole measurement process */
    // two led flashes requested since power has increased
    mock().expectOneCall("Flasher::stopAfter")
        .withParameter("n", 2);
    // second call expected when timer is reset
    mock().expectOneCall("millis").andReturnValue(mockTime);
    StateMachine_Update(mockLifeTester);
    CHECK_EQUAL(ok, mockLifeTester->error);
    POINTERS_EQUAL(StateWaitForTrackingDelay, mockLifeTester->state);
    mock().checkExpectations();
}
#endif