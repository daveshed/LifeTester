/*
 * CPOL = 0
 * CPHA = 0 Data read on falling clock edge
 * SPI_MODE0
 */

#define DEBUG               0         //gives you additional print statements
#define CS_DELAY            100u        //delay between CS logic and data read (us)
#define CONVERSION_TIME     400u      //measurement conversion time (ms) - places upper limit on measurement rate
#define CONVERSION_FACTOR   0.0625    //deg C per code
#define MSB_OVERTEMP        B00111110 //reading this on MSB implies overtemperature
#define SPI_SETTINGS SPISettings(7000000, MSBFIRST, SPI_MODE0)

/* TODO: static bool would be better here but because ino files are just concatenated
 *  we just end up redefining variables when two have ths same name...
 *  bool errorCondition = false; would be preferable but would conflict with definitions in other files
 *  C would be better here.
 */
bool TC77_errorCondition = false;

//raw uint16_t read from chip without conversion
static uint16_t rawData = 0;

/*
 * function to open SPI connection on required CS pin
 */
static void TC77_OpenSpiConnection(uint8_t chipSelectPin)
{
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(chipSelectPin,LOW);
  delayMicroseconds(CS_DELAY);
}
  
/*
 * function to close SPI connection on required CS pin
 */
static void TC77_CloseSpiConnection(uint8_t chipSelectPin)
{
  delayMicroseconds(CS_DELAY);
  digitalWrite(chipSelectPin,HIGH);
  SPI.endTransaction();
} 

/*
 * Function to read a single two bytes from the TC77 and convert them to uint16_t
 */
static uint16_t TC77_ReadRawData(uint8_t chipSelectPin)
{
  uint8_t  MSB, LSB;
  uint16_t rawData;
  
  //open connection
  TC77_OpenSpiConnection(chipSelectPin);

  //read data
  MSB = SPI.transfer(NULL);
  LSB = SPI.transfer(NULL);

  //close connection. realese SPI
  TC77_CloseSpiConnection(chipSelectPin);

  //pack data into a single uint16_t
  rawData = (uint16_t)((MSB << 8) | LSB);
  
  #if DEBUG
    Serial.print("bytes read: ");
    Serial.print(MSB, BIN);
    Serial.print(" ");
    Serial.println(LSB, BIN);
    Serial.print("Raw data output: ");
    Serial.println(rawData);
  #endif

  return rawData;
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
  
  #if DEBUG
    Serial.print("Raw data (signed int): ");
    Serial.println(signedData);
    Serial.print("Temperature (C): ");
    Serial.println(temperature);
  #endif
  
  return temperature;
}

static bool TC77_IsReady(uint16_t rawData)
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
void TC77_Update(uint8_t chipSelectPin)
{
  static uint32_t  timer   = 0;    //timer set to last measurement
  uint16_t         measurement;
  //update data only if the last measurement was longer than conversion time ago. 
  if ((millis() - timer) > CONVERSION_TIME)
  {
    measurement = TC77_ReadRawData(chipSelectPin);
    //reset the timer
    timer = millis();
  }

  //shift out data to leave only the MSB
  if (TC77_IsOverTemp(measurement >> 8))
  {
    TC77_errorCondition = true;
    Serial.println("TC77 overtemperature error.");
    return NULL;
  }
  else if (!TC77_IsReady(measurement)) 
  {
    #if DEBUG
      Serial.println("TC77 not ready.");
    #endif
    rawData = 0;
  }
  else
  {
    rawData = measurement;
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

