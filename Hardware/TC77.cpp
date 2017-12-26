#include "Arduino.h"  //bitRead and binary
#include "Config.h"
#ifdef DEBUG
  #include "Print.h"
#endif
#include "SpiCommon.h"  // spi functions
#include "SpiConfig.h"  // spi #defines
#include "TC77.h"
#include "TC77Private.h"

STATIC SpiSettings_t tc77SpiSettings = {
    0U,
    CS_DELAY,  // defined in Config.h
    SPI_CLOCK_SPEED,
    SPI_BIT_ORDER,
    SPI_DATA_MODE
};

static bool          TC77_errorCondition;
static uint16_t      rawData;
static uint32_t      timer;    //timer set to last measurement

// reads TC77 data ready bit
static bool TC77_IsReady(uint16_t readReg)
{
  return bitRead(readReg, 2U);
}

// reads MSB returned from TC77 for overtemp warning
static bool TC77_IsOverTemp(uint8_t MSB)
{
  if (MSB >= MSB_OVERTEMP)
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
  uint8_t  MSB, LSB;
  uint16_t currentReading;

  OpenSpiConnection(&tc77SpiSettings);

  //read data
  MSB = SpiTransferByte(0u);
  LSB = SpiTransferByte(0u);

  CloseSpiConnection(&tc77SpiSettings);

  //pack data into a single uint16_t
  currentReading = (uint16_t)((MSB << 8) | LSB);
  
  #ifdef DEBUG
    Serial.print("bytes read: ");
    Serial.print(MSB, BIN);
    Serial.print(" ");
    Serial.println(LSB, BIN);
    Serial.print("Raw data output: ");
    Serial.println(currentReading);
  #endif

  return currentReading;
}

void TC77_Init(uint8_t pin)
{
  tc77SpiSettings.chipSelectPin = pin;
  InitChipSelectPin(pin);

  TC77_errorCondition = false;
  rawData = 0U;
  timer = 0U;
}

float TC77_ConvertToTemp(uint16_t rawData)
{
  int16_t signedData;
  float   temperature;
  //remove 3 least sig bits convert to signed
  signedData = (int16_t)(rawData >> 3);
  //convert to temperature
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
  uint16_t currentReading;
  //update data only if the last measurement was longer than conversion time ago. 
  if ((millis() - timer) > CONVERSION_TIME)
  {
    currentReading = TC77_ReadRawData();
    //reset the timer
    timer = millis();
  }

  //shift out data to leave only the MSB
  if (TC77_IsOverTemp(currentReading >> 8))
  {
    // rawData is not updated.
    TC77_errorCondition = true;
    #ifdef DEBUG
      Serial.println("TC77 overtemperature error.");
    #endif
  }
  else if (!TC77_IsReady(currentReading)) 
  {
    // rawData is not updated.
    #ifdef DEBUG
      Serial.println("TC77 not ready.");
    #endif
  }
  else
  {
    rawData = currentReading;
  }
}

uint16_t TC77_GetRawData(void)
{
  return rawData;
}

bool TC77_GetError(void)
{
  return TC77_errorCondition;
}