#include "Config.h"
#include "Print.h"
#include "SPI.h"  //for spi #defines
#include "SpiCommon.h"
#include "TC77.h"

#define TC77_DEBUG          1         //gives you additional print statements
#define CONVERSION_TIME     400u      //measurement conversion time (ms) - places upper limit on measurement rate
#define CONVERSION_FACTOR   0.0625    //deg C per code
#define MSB_OVERTEMP        B00111110 //reading this on MSB implies overtemperature
#define SPI_CLOCK_SPEED     7000000     
#define SPI_BIT_ORDER       MSBFIRST
#define SPI_DATA_MODE       SPI_MODE0

static bool     TC77_errorCondition = false;
static uint16_t rawData             = 0u;
static uint8_t  csPin               = 0u;

static uint16_t TC77_ReadRawData(void);

/*
 * Function to read a single two bytes from the TC77 and convert them to uint16_t
 */
static uint16_t TC77_ReadRawData(void)
{
  uint8_t  MSB, LSB;
  uint16_t currentReading;

  OpenSpiConnection(csPin, CS_DELAY, SPI_CLOCK_SPEED, SPI_BIT_ORDER, SPI_DATA_MODE);

  //read data
  MSB = SPI.transfer(0u);
  LSB = SPI.transfer(0u);

  CloseSpiConnection(csPin, CS_DELAY);

  //pack data into a single uint16_t
  currentReading = (uint16_t)((MSB << 8) | LSB);
  
  #if TC77_DEBUG
    Serial.print("bytes read: ");
    Serial.print(MSB, BIN);
    Serial.print(" ");
    Serial.println(LSB, BIN);
    Serial.print("Raw data output: ");
    Serial.println(currentReading);
  #endif

  return currentReading;
}

void TC77_Init(uint8_t chipSelectPin)
{
  SpiInit(chipSelectPin);
  csPin = chipSelectPin;
}

/*
 * Function to convert the rawData uint16_t to the actual temperature as a float
 */
float TC77_ConvertToTemp(uint16_t rawData)
{
  int16_t signedData;
  float   temperature;
  //remove 3 least sig bits convert to signed
  signedData = (int16_t)(rawData >> 3);
  //convert to temperature
  temperature = (float)signedData * CONVERSION_FACTOR;
  
  #if TC77_DEBUG
    Serial.print("Raw data (signed int): ");
    Serial.println(signedData);
    Serial.print("Temperature (C): ");
    Serial.println(temperature);
  #endif
  
  return temperature;
}

static bool TC77_IsReady(void)
{
  return bitRead(rawData, 2);
}

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
/*
 * Function to update static rawData variable with current temperature. It can 
 * only happen once CONVERSION_TIME has elapsed.
 */
void TC77_Update(void)
{
  uint16_t currentReading;
  static uint32_t  timer   = 0;    //timer set to last measurement
  //update data only if the last measurement was longer than conversion time ago. 
  if ((millis() - timer) > CONVERSION_TIME)
  {
    currentReading = TC77_ReadRawData();
    //reset the timer
    timer = millis();
  }

  //shift out data to leave only the MSB
  if (TC77_IsOverTemp(rawData >> 8))
  {
    TC77_errorCondition = true;
    Serial.println("TC77 overtemperature error.");
  }
  else if (!TC77_IsReady()) 
  {
    #if TC77_DEBUG
      Serial.println("TC77 not ready.");
    #endif
    // rawData is not updated.
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
// undefine here so we don't end up with settings from another device
#undef SPI_CLOCK_SPEED
#undef SPI_BIT_ORDER
#undef SPI_DATA_MODE
