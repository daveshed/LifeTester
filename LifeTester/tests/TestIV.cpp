// CppUnit Test framework
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Code under test
#include "IV.h"

// support
#include "Arduino.h"       // arduino function prototypes - implemented MockAduino.c
#include "Config.h"
#include <stdio.h>

// defines the measurement window in IV scanning or MPPT update
typedef enum MeasurementStage_e {
    settle,
} MeasurementStage_t;

unsigned long millis(void)
{
    mock().actualCall("millis");
    uint32_t retval = mock().unsignedLongIntReturnValue();
    printf("test millis = %u\n", retval);
    return retval;
}

/*******************************************************************************
 * Private function implementations for tests
 ******************************************************************************/

#if 0
// Mocks needed for reading data in update function
static void MockForTC77ReadRawData(void)
{
    mock().expectOneCall("OpenSpiConnection")
        .withParameter("settings", &tc77SpiSettings);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("SpiTransferByte")
        .withParameter("byteToSpiBus", 0U);
    mock().expectOneCall("CloseSpiConnection")
        .withParameter("settings", &tc77SpiSettings);
}
#endif
/*******************************************************************************
 * Unit tests
 ******************************************************************************/
// Define a test group for IV - all tests share common setup/teardown
TEST_GROUP(IVTestGroup)
{
    void setup(void)
    {

    }

    void teardown(void)
    {
        mock().clear();
    }
};

TEST(IVTestGroup, RunIvScan)
{
    // mock call to instantiate flasher object
    mock().expectOneCall("Flasher")
        .withParameter("pin", LED_A_PIN);

    // mock lifetester data
    LifeTester_t LTChannelA =
    {
        {chASelect, 0U},
        Flasher(LED_A_PIN),
        0,
        0,
        0,
        ok,
        {0},
        0
    };

    const uint32_t tInitial = 348775U;
    uint32_t       mockTime = tInitial;
    uint32_t       tPrevious = mockTime;
    const uint32_t dt = 50U; // ms

    // IV scan setup - mock calls that happen prior to IV scan loop
    mock().expectOneCall("millis").andReturnValue(tInitial);
    mock().expectOneCall("Flasher::t")
        .withParameter("onNew", SCAN_LED_ON_TIME)
        .withParameter("offNew", SCAN_LED_OFF_TIME);
    
    // Note that voltages are sent as dac codes right now
    const uint16_t vInitial = 0U;
    const uint16_t vFinal   = 9U;
    const uint16_t dV       = 3U;
    uint16_t       mockVolts = vInitial;

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
                .withParameter("channel", LTChannelA.channel.dac);
        }
        // (2) Sample current during measurement time window
        else if((tElapsed >= SETTLE_TIME) && (tElapsed < (SETTLE_TIME + SAMPLING_TIME)))
        {
            mock().expectOneCall("AdcReadData")
                .withParameter("channel", LTChannelA.channel.adc)
                .andReturnValue(10U); // Requires diode equation or similar
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
        .withParameter("output", 0U)
        .withParameter("channel", LTChannelA.channel.dac);

    IV_Scan(&LTChannelA, 0, 9, 3);

    FAIL("Fail me!");
    mock().checkExpectations();
}

#if 0
// Test for initialising TC77 device.
TEST(TC77TestGroup, InitialiseTC77)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    CHECK_EQUAL(CS_DELAY, tc77SpiSettings.chipSelectDelay);
    CHECK_EQUAL(SPI_CLOCK_SPEED, tc77SpiSettings.clockSpeed);
    CHECK_EQUAL(SPI_BIT_ORDER, tc77SpiSettings.bitOrder);
    CHECK_EQUAL(SPI_DATA_MODE, tc77SpiSettings.dataMode);

    // check function calls
    mock().checkExpectations();
}
/*
 Test for initialising then calling update function on TC77 device before 
 conversion has taken place. So 0 should be returned.
 */
TEST(TC77TestGroup, ReadingTC77BeforeConversionNoData)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    mock().expectOneCall("millis");
    TC77_Update();
    CHECK_EQUAL(0U, TC77_GetRawData());   
    mock().checkExpectations();
}

/*
 Test for initialising then calling update function on TC77 device before 
 the device is ready - reading the ready bit of the read register. Expect 0 to be
 returned even though there is data in the register ie. initial data doesn't get
 updated and 0 remains.
 */
TEST(TC77TestGroup, ReadingTC77AfterConversionNotReady)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    mock().expectOneCall("millis");
    
    // Put data into read register - do not expect to see this. Device not ready.
    const float mockTemperature = 25.2F;
    // Now update read reg but set state to busy
    UpdateReadReg(mockTemperature, false);
    TC77_Update();
    
    CHECK_EQUAL(0U, TC77_GetRawData());   
    mock().checkExpectations();
}

/*
 Test for calling update function on TC77 device after conversion time. We expect
 data to be available and for it to be returned over spi bus. 
 */
TEST(TC77TestGroup, ReadingTC77AfterConversionExpectData)
{
    const uint8_t pinNum = 1U;
    // Mock calls to low level spi function for init
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    TC77_Init(pinNum);

    // Do not expect error condition
    CHECK(!TC77_GetError());

    // Put data into read register
    const float mockTemperature = 25.2F;
    UpdateReadReg(mockTemperature, true);
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;
    
    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    
    TC77_Update();

    // Check data returned against expectations
    const uint16_t rawDataActual = TC77_GetRawData();
    const uint16_t rawDataExpected = GetSpiReadReg();
    CHECK_EQUAL(rawDataExpected, rawDataActual);
    DOUBLES_EQUAL(mockTemperature, TC77_ConvertToTemp(rawDataActual), 0.1);
    // Do not expect error condition
    CHECK(!TC77_GetError());
    
    // Checking mock function calls
    mock().checkExpectations();
}

/*
 Test for calling update function on TC77 device after conversion time with an
 overtemp condition. Data should be returned but with error condition must be
 set to true.
 */
TEST(TC77TestGroup, ReadingTC77AfterConversionWithOverTemp)
{
    const uint8_t pinNum = 1U;
    
    // Mock calls to low level spi function for init
    mock().expectOneCall("InitChipSelectPin")
        .withParameter("pin", pinNum);
    
    // Call init function with required pin setting - chip select
    TC77_Init(pinNum);
    // Should be no error condition at this point.
    CHECK(!TC77_GetError());

    /* max temperature is 125C. So setting the temperature above this should
    trigger an overtemperature error */
    const float mockTemperature = 128.4F;  // f for float not farenheit!  
    /* Now that the temperature is set, we can update the contents of the TC77's
    read register with a mock value that corresponds to it.*/
    UpdateReadReg(mockTemperature, true);
    // increment timer to take us over the conversion time.
    mockMillis = CONVERSION_TIME + 10U;

    // mock spi calls when data is read from TC77 device.
    mock().expectOneCall("millis");
    MockForTC77ReadRawData();
    
    // now call the update function
    TC77_Update();

    // Compare returned data against expectations
    const uint16_t rawDataExpected = GetSpiReadReg();
    const uint16_t rawDataActual = TC77_GetRawData();
    CHECK_EQUAL(rawDataExpected, rawDataActual);
    DOUBLES_EQUAL(mockTemperature, TC77_ConvertToTemp(rawDataActual), 0.1);
    
    // expect error condition because of over temperature
    CHECK(TC77_GetError());

    // Checking mock function calls
    mock().checkExpectations();
}
#endif