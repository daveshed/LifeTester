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

static void MX7705_Write(uint8_t sendByte);
static uint8_t MX7705_ReadByte(void);

/*
 * Setup the MX7705 in unipolar, unbuffered mode. Allow the user to 
 * select which channel they want. 0/1 = AIN1+ to AIN1-/AIN2+ to AIN2-.
 */
void MX7705_Init(const uint8_t pin, const uint8_t channel)
{
    // set the value of the chip select pin in mx7705SpiSettings global
    mx7705SpiSettings.chipSelectPin = pin;

    InitChipSelectPin(pin);
#ifndef UNIT_TEST  // TODO fix tests. Need definitions to remove compile guards
    pinMode(PWMout, OUTPUT);
    //sending clock pulses from pin3 at 1MHz to ADC
    //see http://forum.arduino.cc/index.php?topic=22384.0
    TCCR2A = 0xB3; // fast PWM with programmed TOP val
    TCCR2B = 0x09; // divide by 1 prescale
    TCNT2  = 0x00;
    OCR2A  = 0x0F; // TOP = 15, cycles every 16 clocks
    OCR2B  = 0x07; // COMP for pin3
#endif
    //request a write to the clock register
    MX7705_Write(MX7705_REQUEST_CLOCK_WRITE);
    //turn on clockdis bit - using clock from ATMEGA. Turn off clk bit for optimum performance at 1MHz with clkdiv 0.
    MX7705_Write(MX7705_WRITE_CLOCK_SETTINGS);
    //write to comms register: request a write operation to the setup register of selected channel
    MX7705_Write(channel == 0U ? MX7705_REQUEST_SETUP_WRITE_CH0 : MX7705_REQUEST_SETUP_WRITE_CH1);
    //write to setup register: self calibration mode, unipolar, unbuffered, clear Fsync
    MX7705_Write(MX7705_WRITE_SETUP_INIT);
    //request read of setup register to verify state
    MX7705_Write(channel == 0U ? MX7705_REQUEST_SETUP_READ_CH0 : MX7705_REQUEST_SETUP_READ_CH1);

    //now read setup register and verify that we have written the correct setup state
    if (MX7705_ReadByte() != MX7705_WRITE_SETUP_INIT)
    {
        MX7705_errorCondition = true;
        #ifdef DEBUG
            Serial.println("MX7705: Error condition");
        #endif
    }

}

/*
 * Function to get the state of the error condition
 */
bool MX7705_GetError(void)
{
  return MX7705_errorCondition;
}

/*
 * Function to read two bytes from the ADC and convert to a 16Bit unsigned int  
 */
uint16_t MX7705_ReadData(const uint8_t channel)
{
  uint8_t   MSB, LSB, commsRegister;
  uint32_t  tic, toc;
  uint16_t  pollCount = 0u;
  bool timeout = false;
  
  MSB = 0u;
  LSB = 0u;
  commsRegister = 0u;
  
  tic = millis();
  
  do
  {
    //polling DRDYpin bit of comms register waiting for measurement to finish    
    //DRDY = 0/1 measurement not/is ready
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
    MSB = MX7705_ReadByte();
    LSB = MX7705_ReadByte();
    return (uint16_t)((MSB << 8) | LSB);
  }
}

/*
 * Function to read gain of a given channel
 */
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

/*
 * Function to set gain of the MX7705. Note that gain is set as an unsigned int from 0 - 7 inclusive
 */
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

/*
 * Function to increase the gain of selected channel by one step
 */
 //TODO: function not properly implemented
void MX7705_GainUp(const uint8_t channel)
{
  MX7705_Write(channel == 0u ? B00001000 : B00001001);
}

/*
 * function to send a byte over SPI to the MX7705
 */
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
/*
 * Function to read a single byte from the MX7705
 */
static uint8_t MX7705_ReadByte(void)
{
  uint8_t readByte = 0u;
  
  OpenSpiConnection(&mx7705SpiSettings);
  
  readByte = SpiTransferByte(0u);
  
  #if DEBUG
    Serial.print("MX7705: Data received... ");
    Serial.println(readByte, BIN);
  #endif
  
  CloseSpiConnection(&mx7705SpiSettings);
  
  return readByte;
}