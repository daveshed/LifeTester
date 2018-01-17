#include "Arduino.h"
#include "Config.h"
#include "MCP4802.h"
#include "MCP4802Private.h"
#include "Print.h"
#include "SpiCommon.h"

static gainSelect_t gain;

SpiSettings_t MCP4802SpiSettings = {
    0U,
    CS_DELAY,       // defined in Config.h
    SPI_CLOCK_SPEED,// default values
    SPI_BIT_ORDER,
    SPI_DATA_MODE
};

// Gets the dac command that controls the device
STATIC uint16_t MCP4802_GetDacCommand(chSelect_t   ch,
                                      gainSelect_t gain,
                                      shdnSelect_t shdn,
                                      uint8_t      output)
{
    uint16_t reg = 0U;
    bitWrite(reg, CH_SELECT_BIT, ch);
    bitWrite(reg, GAIN_SELECT_BIT, gain);
    bitWrite(reg, SHDN_BIT, shdn);
    bitInsert(reg, output, DATA_MASK, DATA_OFFSET);
    return reg;
}

// Sends binary Spi command to the dac
static void SendSpiCommand(uint16_t command)
{
    OpenSpiConnection(&MCP4802SpiSettings);
    const uint8_t msb = ((command >> 8U) & 0xFF); 
    const uint8_t lsb = (command & 0xFF); 
    SpiTransferByte(msb);
    SpiTransferByte(lsb);
    CloseSpiConnection(&MCP4802SpiSettings);
}

void MCP4802_Init(uint8_t pin)
{
    MCP4802SpiSettings.chipSelectPin = pin;
    gain = lowGain;
    InitChipSelectPin(pin);

    MCP4802_SetGain(gain);
    MCP4802_Output(0u, chASelect);
    MCP4802_Output(0u, chBSelect);
}

void MCP4802_Output(uint8_t output, chSelect_t ch)
{
    const uint16_t dacCommand = 
        MCP4802_GetDacCommand(ch, gain, shdnOff, output);

    SendSpiCommand(dacCommand);

    #if DEBUG
      Serial.print("MCP4802 sending: ");
      Serial.println(dacCommand, BIN);
    #endif
    
}

void MCP4802_Shutdown(chSelect_t ch)
{
    SendSpiCommand(MCP4802_GetDacCommand(ch, gain, shdnOn, 0U));
}

void MCP4802_SetGain(gainSelect_t requestedGain)
{
    gain = requestedGain;
}

gainSelect_t MCP4802_GetGain(void)
{
    return gain;
}