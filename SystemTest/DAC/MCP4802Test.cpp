#include "Arduino.h"  // string definition
#include "Config.h"
#include "MCP4802.h"
#include "SpiCommon.h"

#define DELAY_TIME_MS   (1000U)
#define V_REF           (2.048F)  // internal reference voltage
#define CODE_STEP_SIZE  (10U)
#define DAC_MAX_CODE    (0xFF)    // 8 bit dac. This code gives Vref

typedef struct mockDacChannel_s {
    uint16_t     output;
    shdnSelect_t shdnMode;
} dacChannel_t;

typedef struct mockDacState_s {
    gainSelect_t gainMode;     // on/off = low/high
    dacChannel_t chA;          // channel A output
    dacChannel_t chB;          // channel B output
} dacState_t;

static void PrintDacStatus(dacState_t *dac)
{
    const float voltageA = (float)dac->chA.output * V_REF / (float)DAC_MAX_CODE;
    const float voltageB = (float)dac->chB.output * V_REF / (float)DAC_MAX_CODE;
    const String shdnA = (dac->chA.shdnMode == shdnOn) ? "Off   " : "Active";
    const String shdnB = (dac->chB.shdnMode == shdnOn) ? "Off    " : "Active ";
    const String gain = (dac->gainMode == lowGain) ? "Low   " : "High  ";
    const String data = String(String(shdnA) + "/" + String(shdnB) +
                               gain + 
                               String(dac->chA.output) + "/" + String(dac->chB.output) + "   " +
                               String(voltageA) + "/" + String(voltageB));

    Serial.println(data);
}

void setup()
{
    Serial.begin(9600);
    SpiBegin();
    MCP4802_Init(DAC_CS_PIN);
    Serial.println("shdn(A/B)     gain  code(A/B) voltage(A/B)");
}

void loop()
{
    static uint8_t    dacACode = 0U;
    static uint8_t    dacBCode = 0U;
    static dacState_t dac = {
        lowGain,
        {0U, shdnOff},
        {0U, shdnOff}
    };
    static uint16_t counter = 0U;

    MCP4802_Output(dacACode, chASelect);
    MCP4802_Output(dacBCode, chBSelect);


    dac.chA.output = dacACode;
    dac.chB.output = dacBCode;

    PrintDacStatus(&dac);
    delay(DELAY_TIME_MS);
    dacACode += CODE_STEP_SIZE;
    dacBCode += CODE_STEP_SIZE * 2U;  // make this increment faster
}
