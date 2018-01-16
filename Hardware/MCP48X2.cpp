#include "Arduino.h"
#include "Config.h"
#include "MCP48X2.h"
#include "MCP48X2Private.h"
#include "Print.h"
#include "SpiCommon.h"

static gainSelect_t gain;

SpiSettings_t mcp48x2SpiSettings = {
    0U,
    CS_DELAY,       // defined in Config.h
    SPI_CLOCK_SPEED,// default values
    SPI_BIT_ORDER,
    SPI_DATA_MODE
};

// Gets the dac command that controls the device
STATIC uint16_t MCP48X2_GetDacCommand(chSelect_t   ch,
                                      gainSelect_t gain,
                                      shdnSelect_t shdn,
                                      uint16_t     output)
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
    OpenSpiConnection(&mcp48x2SpiSettings);
    const uint8_t msb = ((command >> 8U) & 0xFF); 
    const uint8_t lsb = (command & 0xFF); 
    SpiTransferByte(msb);
    SpiTransferByte(lsb);
    CloseSpiConnection(&mcp48x2SpiSettings);
}

void MCP48X2_Init(uint8_t pin)
{
    mcp48x2SpiSettings.chipSelectPin = pin;
    gain = lowGain;
    InitChipSelectPin(pin);

    MCP48X2_SetGain(gain);
    MCP48X2_Output(0u, chASelect);
    MCP48X2_Output(0u, chBSelect);
}

void MCP48X2_Output(uint8_t output, chSelect_t ch)
{
    const uint16_t dacCommand = 
        MCP48X2_GetDacCommand(ch, gain, shdnOff, output);

    SendSpiCommand(dacCommand);

    #if DEBUG
      Serial.print("MCP48X2 sending: ");
      Serial.println(dacCommand, BIN);
    #endif
    
}

void MCP48X2_Shutdown(chSelect_t ch)
{
    SendSpiCommand(MCP48X2_GetDacCommand(ch, gain, shdnOn, 0U));
}

void MCP48X2_SetGain(gainSelect_t requestedGain)
{
    gain = requestedGain;
}

gainSelect_t MCP48X2_GetGain(void)
{
    return gain;
}