#include "Arduino.h"  //bitRead and binary
#include "Config.h"
#ifdef DEBUG
  #include "Print.h"
#endif
#include "SpiCommon.h"  // spi functions
#include "SpiConfig.h"  // spi #defines
#include "TC77.h"
#include "TC77Private.h"

SpiSettings_t tc77SpiSettings = {
    0U,
    CS_DELAY,  // defined in Config.h
    SPI_CLOCK_SPEED,
    SPI_BIT_ORDER,
    SPI_DATA_MODE
};

static bool     errorCondition;
static uint16_t previousReading;
static uint32_t previousTime;    //previousTime set to last measurement

// reads TC77 data ready bit
static bool TC77_IsReady(uint16_t readReg)
{
  return bitRead(readReg, 2U);
}

// reads MSB returned from TC77 for overtemp warning
static bool TC77_IsOverTemp(uint8_t msb)
{
  if (msb >= MSB_OVERTEMP)
  {
    return true;
  }
  else
  {
    return false;
  }
}

// reads raw data from TC77 for conversion elsewhere
static uint16_t TC77_ReadRawData(void)
{
  uint8_t msb; // most sig byte read first
  uint8_t lsb; // then least sig
  uint16_t readReg;

  OpenSpiConnection(&tc77SpiSettings);

  //read data
  msb = SpiTransferByte(0u);
  lsb = SpiTransferByte(0u);

  CloseSpiConnection(&tc77SpiSettings);

  //pack data into a single uint16_t
  readReg = (uint16_t)((msb << 8U) | lsb);
  
  #ifdef DEBUG
    Serial.print("bytes read: ");
    Serial.print(msb, BIN);
    Serial.print(" ");
    Serial.println(lsb, BIN);
    Serial.print("Raw data output: ");
    Serial.println(readReg);
  #endif

  return readReg;
}

void TC77_Init(uint8_t pin)
{
  tc77SpiSettings.chipSelectPin = pin;
  InitChipSelectPin(pin);

  errorCondition = false;
  previousReading = 0U;
  previousTime = 0U;
}

float TC77_ConvertToTemp(uint16_t readReg)
{
  int16_t signedData;
  float   temperature;
  // Remove 3 least sig bits convert to signed
  signedData = (int16_t)(readReg >> 3U);
  // Convert to temperature using conversion factor
  temperature = (float)signedData * CONVERSION_FACTOR;
  
  #ifdef DEBUG
    Serial.print("Raw data (signed int): ");
    Serial.println(signedData);
    Serial.print("Temperature (C): ");
    Serial.println(temperature);
  #endif
  
  return temperature;
}

void TC77_Update(void)
{
    const uint32_t currentTime = millis();
    
    //update data only if the last measurement was longer than conversion time ago. 
    if ((currentTime - previousTime) > CONVERSION_TIME)
    {
        uint16_t currentReading;
        currentReading = TC77_ReadRawData();
        //reset the previousTime
        previousTime = currentTime;

        // Now check the reading - should we update our data or not?
        if (TC77_IsOverTemp(currentReading >> 8U))
        {
            // previousReading is not updated.
            errorCondition = true;
            #ifdef DEBUG
            Serial.println("TC77 overtemperature error.");
            #endif
            previousReading = currentReading;
        }
        else if (!TC77_IsReady(currentReading)) 
        {
            // previousReading is not updated.
            #ifdef DEBUG
            Serial.println("TC77 not ready.");
            #endif
        }
        else
        {
            // only update if there are no error conditions
            previousReading = currentReading;
        }
    }
}

uint16_t TC77_GetRawData(void)
{
  return previousReading;
}

bool TC77_GetError(void)
{
  return errorCondition;
}