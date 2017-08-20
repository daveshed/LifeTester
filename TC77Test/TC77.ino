/*
 * CPOL = 0
 * CPHA = 0 Data read on falling clock edge
 * SPI_MODE0
 */

#define DEBUG               1         //gives you additional print statements
#define CS_DELAY            100u        //delay between CS logic and data read (us)
#define CONVERSION_TIME     400u      //measurement conversion time (ms) - places upper limit on measurement rate
#define CONVERSION_FACTOR   0.0625    //deg C per code
#define MSB_OVERTEMP        B00111110 //reading this on MSB implies overtemperature
#define SPI_SETTINGS SPISettings(7000000, MSBFIRST, SPI_MODE0)

/*
 * function to open SPI connection on required CS pin
 */
void TC77OpenSpiConnection(uint8_t chipSelectPin)
{
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(chipSelectPin,LOW);
  delayMicroseconds(CS_DELAY);
}
  
/*
 * function to close SPI connection on required CS pin
 */
void TC77CloseSpiConnection(uint8_t chipSelectPin)
{
  delayMicroseconds(CS_DELAY);
  digitalWrite(chipSelectPin,HIGH);
  SPI.endTransaction();
} 

/*
 * Function to read a single byte from the TC77
 */
uint16_t TC77ReadRawData(uint8_t chipSelectPin)
{
  uint8_t  MSB, LSB;
  uint16_t rawData;
  
  //open connection
  TC77OpenSpiConnection(chipSelectPin);

  //read data
  MSB = SPI.transfer(NULL);
  LSB = SPI.transfer(NULL);

  //close connection. realese SPI
  TC77CloseSpiConnection(chipSelectPin);

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

float TC77ConvertToTemp(uint16_t rawData)
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

bool TC77IsReady(uint16_t rawData)
{
  return bitRead(rawData, 2);
}

bool TC77IsOverTemp(uint8_t MSB)
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

float TC77ReadTemp(uint8_t chipSelectPin)
{
  static uint16_t  rawData = 0;
  static uint32_t  timer   = 0;    //timer set to last measurement

  //update data only if the last measurement was longer than conversion time ago. 
  if ((millis() - timer) > CONVERSION_TIME)
  {
    rawData = TC77ReadRawData(chipSelectPin);
    //reset the timer
    timer = millis();
  }

  //shift out data to leave only the MSB
  if (TC77IsOverTemp(rawData >> 8))
  {
    Serial.println("TC77 overtemperature error.");
    return NULL;
  }
  else if (!TC77IsReady(rawData)) 
  {
    #if DEBUG
      Serial.println("TC77 not ready.");
    #endif
    return 0;
  }
  else
  {
    return TC77ConvertToTemp(rawData);
  }
}

