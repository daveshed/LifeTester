#include "Arduino.h"
#include "Config.h"
#include "MX7705.h"
#include "MX7705Private.h"

SpiSettings_t mx7705SpiSettings = {
    0U,
    CS_DELAY,       // defined in Config.h
    SPI_CLOCK_SPEED,// default values
    SPI_BIT_ORDER,
    SPI_DATA_MODE
};

static bool MX7705_errorCondition = false;

// Sends a single byte over SPI to the MX7705
static void MX7705_Write(uint8_t sendByte)
{ 
  OpenSpiConnection(&mx7705SpiSettings);
  
  #if DEBUG
    Serial.print("MX7705: Sending... ");
    Serial.println(sendByte, BIN);
  #endif
  
  SpiTransferByte(sendByte);
  
  CloseSpiConnection(&mx7705SpiSettings);
}

// Reads a single byte from the MX7705 over SPI
static uint8_t MX7705_ReadByte(void)
{
  OpenSpiConnection(&mx7705SpiSettings);
  
  const uint8_t readByte = SpiTransferByte(0u);
  
  #if DEBUG
    Serial.print("MX7705: Data received... ");
    Serial.println(readByte, BIN);
  #endif
  
  CloseSpiConnection(&mx7705SpiSettings);
  
  return readByte;
}

// Reads a uint16_t over spi. Used in reading data following voltage conversion.
static uint16_t MX7705_Read16Bit(void)
{
  OpenSpiConnection(&mx7705SpiSettings);
  
  const uint8_t msb = SpiTransferByte(0u);
  const uint8_t lsb = SpiTransferByte(0u);
  const uint16_t retVal = (msb << 8U) | lsb;

  #if DEBUG
    Serial.print("MX7705: Data received... ");
    Serial.print(msb, BIN);
    Serial.println(lsb, BIN);
  #endif
  
  CloseSpiConnection(&mx7705SpiSettings);
  return retVal;
}

#ifndef UNIT_TEST
    /*
     Sending clock pulses from pin3 at 1MHz to ADC
     see http://forum.arduino.cc/index.php?topic=22384.0
    */
    static void InitClockOuput(uint8_t pin)
    {
        pinMode(PWMout, OUTPUT);
        TCCR2A = 0xB3; // fast PWM with programmed TOP val
        TCCR2B = 0x09; // divide by 1 prescale
        TCNT2  = 0x00;
        OCR2A  = 0x0F; // TOP = 15, cycles every 16 clocks
        OCR2B  = 0x07; // COMP for pin3
    }
#endif // UNIT_TEST

/*
 Generates a comms register command to request a register to read or write for 
 a given channel. 
*/
STATIC uint8_t RequestRegCommand(RegisterSelection_t demandedReg,
                                 uint8_t             channel,
                                 bool                readOp)
{
    uint8_t commsRegister = 0U;
    // write in requested register
    bitInsert(commsRegister, demandedReg, REG_SELECT_MASK, REG_SELECT_OFFSET);
    // set channel
    bitInsert(commsRegister, channel, CH_SELECT_MASK, CH_SELECT_OFFSET);
    // 0 = write, 1 = read operation
    bitWrite(commsRegister, RW_BIT, readOp);
    // now return the comms register command
    return commsRegister;
}

STATIC uint8_t RequestRegRead(RegisterSelection_t demandedReg, uint8_t channel)
{
    return RequestRegCommand(demandedReg, channel, true);
}

STATIC uint8_t RequestRegWrite(RegisterSelection_t demandedReg, uint8_t channel)
{
    return RequestRegCommand(demandedReg, channel, false);
}

/*
 Sets up the adc clock for the channel specified in by the reg request command.
 Use this to set the clock settings after requesting a write to the clock reg.
*/
STATIC uint8_t SetClockSettings(bool    clockOutDisable,    // disables clock out signals
                                bool    internalClockDivOn, // divide by two otherwise divide by 1
                                bool    lowFreqClkIn,       // 0 = 4.9MHz 1 = 2.5MHz clock in 
                                uint8_t filterSelection)    // determines output data rate and cutoff freq 
{
    uint8_t clockRegister = 0U;
    bitWrite(clockRegister, CLKDIS_BIT, clockOutDisable);
    bitWrite(clockRegister, CLKDIV_BIT, internalClockDivOn);
    bitWrite(clockRegister, CLK_BIT, lowFreqClkIn);
    bitInsert(clockRegister, filterSelection, FILTER_SELECT_MASK, FILTER_SELECT_OFFSET);

    /* TODO - this bit does not need to be set. It's read only. Preserving
    original behaviour of code. */
    bitSet(clockRegister, MXID_BIT);
    return clockRegister;
}

/*
 Sets up the adc setup register for measurements following a setup register
 write request made into the comms register.
*/
STATIC uint8_t SetSetupSettings(AdcMode_t mode,
                                uint8_t pgaGainIdx, // gain setting
                                bool unipolarMode,  // unipolar/bipolar mode
                                bool enableBuffer,  // useful with high source impedance inputs
                                bool filterSync)    // set this to syncronize measurements with filter
{
    uint8_t setupRegister = 0U;
    bitInsert(setupRegister, mode, MODE_MASK, MODE_OFFSET);
    bitInsert(setupRegister, pgaGainIdx, PGA_MASK, PGA_OFFSET);
    bitWrite(setupRegister, BIPOLAR_UNIPOLAR_BIT, unipolarMode);
    bitWrite(setupRegister, BUFFER_BIT, enableBuffer);
    bitWrite(setupRegister, FSYNC_BIT, filterSync);
    return setupRegister;
}

void MX7705_Init(const uint8_t pin, const uint8_t channel)
{
    MX7705_errorCondition = false;
    // set the value of the chip select pin in mx7705SpiSettings global
    mx7705SpiSettings.chipSelectPin = pin;

    InitChipSelectPin(pin);
#ifndef UNIT_TEST  // TODO fix tests. Need definitions to remove compile guards
    InitClockOuput(PWMout);
#endif
    // Request a write to the clock register
    MX7705_Write(RequestRegWrite(ClockReg, channel));
    /*
     turn on clockdis bit - using clock from ATMEGA. Turn off clk bit for optimum
     performance at 1MHz with clkdiv 0.
     */
    const uint8_t clockRegToSet = SetClockSettings(true, false, false, 1U);
    MX7705_Write(clockRegToSet);
    // Writing to comms reg. Request a write operation to the setup register
    MX7705_Write(RequestRegWrite(SetupReg, channel));
    // Write to setup register - self calibration mode, unipolar, unbuffered, clear Fsync
    const uint8_t setupRegToSet = SetSetupSettings(SelfCalibMode, 0U, true, false, false);
    MX7705_Write(setupRegToSet);

    /* Now read setup and clock registers to verify that we have written the
     correct settings and communication is working ok. */
    MX7705_Write(RequestRegRead(ClockReg, channel));
    const uint8_t clockRegRead = MX7705_ReadByte();
    
    MX7705_Write(RequestRegRead(SetupReg, channel));
    const uint8_t setupRegRead = MX7705_ReadByte();
    
    // Check that data read back matches expectations
    if ((setupRegRead != setupRegToSet) || (clockRegRead != clockRegToSet))
    {
        MX7705_errorCondition = true;
        #ifdef DEBUG
            Serial.println("MX7705: Error condition");
        #endif
    }
}

bool MX7705_GetError(void)
{
    return MX7705_errorCondition;
}

uint16_t MX7705_ReadData(const uint8_t channel)
{
    uint8_t         commsRegister = 0U;
    const uint32_t  tic =  millis();
    uint32_t        toc;
    uint16_t        pollCount = 0U;
    bool            timeout = false;
      
    do
    {
        // polling DRDYpin bit of comms register waiting for measurement to finish    
        // DRDY = 0 is ready / 1 not ready
        toc = millis();
        timeout = ((toc - tic) > TIMEOUT_MS);
        //select read of comms register
        MX7705_Write(channel == 0 ? MX7705_REQUEST_COMMS_READ_CH0 : MX7705_REQUEST_COMMS_READ_CH1);
        commsRegister = MX7705_ReadByte();
        pollCount++;
        //bit 7 of comms register
    } while (bitRead(commsRegister, 7) && !timeout);

    #if DEBUG
        if (pollCount > 0)
        {
            Serial.print("MX7705: DRDY bit polled... ");
            Serial.print(pollCount);
            Serial.println(" times");
        }
    #endif

    if (timeout)
    {
        MX7705_errorCondition = true;
        return 0u;
    }
    else
    {
        //request data register reading
        MX7705_Write((channel == 0 ? MX7705_REQUEST_DATA_READ_CH0 : MX7705_REQUEST_DATA_READ_CH1));

        return MX7705_Read16Bit();
    }
}

uint8_t MX7705_GetGain(const uint8_t channel)
{
  uint8_t setupRegister, gainRegister;
  
  //request read of the setup register of the given channel
  MX7705_Write(channel == 0u ? MX7705_REQUEST_SETUP_READ_CH0 : MX7705_REQUEST_SETUP_READ_CH1);
  
  //now read setup register. Note that gain is indicated by bits 3-5
  setupRegister = MX7705_ReadByte();
  gainRegister = (setupRegister &= B00111000) >> 3u;  //mask bits 3-5 and shift to rightmost bit.
  
  return gainRegister; 
}

void MX7705_SetGain(const uint8_t requiredGain, const uint8_t channel)
{
  uint8_t setupRegister;
  // required gain is unsigned so don't need to check values lower than 0
  if (requiredGain > 7u)
  {
    #if DEBUG
        Serial.print("MX7705: Cannot set gain to ");
        Serial.print(requiredGain);
        Serial.println(". Maximum gain is 7.");
    #endif
  }
  else
  {
    //request a read of the setup register for the given channel - store and amend gain bits. We don't want to change data that's already here.
    MX7705_Write(channel == 0 ? MX7705_REQUEST_SETUP_READ_CH0 : MX7705_REQUEST_SETUP_READ_CH1);
    
    //now read setup register. Note that gain is indicated by bits 3-5
    setupRegister = MX7705_ReadByte();
    
    //write the required gain onto setupRegister and write it back in
    setupRegister &= B11000111;               //delete current gain bits
    setupRegister |= (requiredGain << 3u);     //replace with required gain  - note similarity with GetGain.
    
    //request write to setup register
    MX7705_Write(channel == 0u ? MX7705_REQUEST_SETUP_WRITE_CH0 : MX7705_REQUEST_SETUP_WRITE_CH1);
    
    //write new setupRegister
    MX7705_Write(setupRegister);
  }
}

 /*TODO: function not properly implemented. Need to read the gain, change it then
 set it back again. Then check that it has changed successfully?*/
void MX7705_GainUp(const uint8_t channel)
{
  MX7705_Write(channel == 0u ? B00001000 : B00001001);
}

