/*
 * sketch to be uploaded to lifetime testing master device
 * for reading data from the slave test board
 * data received as a string of 17 bytes:
 
byte[i] data    significance  size
--------------------------------------
0       timer   LSB           uint32_t
1       timer
2       timer
3       timer   MSB
4       v_a     LSB           uint16_t
5       v_a     MSB
6       i_a     LSB           uint16_t
7       i_a     MSB
8       v_b     LSB           uint16_t
9       v_b     MSB
10      i_b     LSB           uint16_t
11      i_b     MSB
12      T       LSB           uint16_t
13      T       MSB
14      intsty  LSB           uint16_t
15      intsty  MSB
16      error_a N/A           uint8_t
17      error_b N/A           uint8_t
*/

#include <Wire.h>

#define MAX_DATA_SIZE       8 //4 bytes for a uint64_t
#define BUFFER_ENTRIES      9
#define BUFFER_MAX_SIZE     MAX_DATA_SIZE * BUFFER_ENTRIES

const uint16_t  tDelay = 100;
uint8_t         I2CByteBuffer[BUFFER_MAX_SIZE] = {0};
uint8_t         bytesToRead;
uint8_t         writeIdx = 0;
String          inputString;  //holds serial commands from user

/*
 * Macro to read an unsigened int from the buffer
 * at the location specified by index, idx.
 * variable returned as uint64_t buffer array of uint8_t ref needed
 */
#define BUFFER_READ(slaveAddress)                                       \
  I2CMaster_ReadBytes(I2CByteBuffer, slaveAddress, bytesToRead);        \
  I2CMaster_ReadByteBuffer(I2CByteBuffer, I2CDataArray, BUFFER_ENTRIES)

/*
 * Macro which clears the bytebuffer and resets the index
*/
#define BUFFER_RST()                                    \
  I2CMaster_ClearBuffer(I2CByteBuffer, BUFFER_MAX_SIZE);\
  writeIdx = 0;                                         \
  I2CMaster_ClearDataArray()
                                                                     
#define BUFFER_PRINT()    \
  I2CMaster_PrintByteArray(I2CByteBuffer, bytesToRead)

//Buffer contents formatting
typedef struct data_s {
  const char* label;
  uint8_t     numBytes;
  uint8_t     idx;
  uint64_t    data;
} data_t;

data_t I2CDataArray[] = {
  {.label = "timer", .numBytes= sizeof(uint32_t)},
  {.label = "v_a", .numBytes= sizeof(uint16_t)},
  {.label = "i_a", .numBytes= sizeof(uint16_t)},
  {.label = "v_b", .numBytes= sizeof(uint16_t)},
  {.label = "i_b", .numBytes= sizeof(uint16_t)},
  {.label = "T", .numBytes= sizeof(uint16_t)},
  {.label = "Intensity", .numBytes= sizeof(uint16_t)},
  {.label = "error_a", .numBytes= sizeof(uint8_t)},
  {.label = "error_b", .numBytes = sizeof(uint8_t)}
};

/*
 * Reset the byte buffer
*/
void I2CMaster_ClearBuffer(uint8_t byteArray[], const uint8_t numBytes)
{
  for (int i = 0; i < numBytes; i++)
  {
    byteArray[i] = 0;
  }
}

/*
 * Reset the data array
 */
void I2CMaster_ClearDataArray(void)
{
  for (int e = 0; e < BUFFER_ENTRIES; e++)                                  
  {
    I2CDataArray[e].data = 0;
  }
}

/*
 * Print the contents of each element of the byte array. Useful for debugging.
 */
void I2CMaster_PrintByteArray(const uint8_t byteArray[], const uint16_t n)
{
  for (int i = 0; i < n; i++)
  {
    Serial.print(byteArray[i]);
    Serial.print(" ");
  }
  Serial.println();
}

/*
 * converts a uint64 into a decimal string for printing out. note that Serial.print does not support uint64_t
 */
String ToString(uint64_t x)
{
     boolean flag = false; // For preventing string return like this 0000123, with a lot of zeros in front.
     String str = "";      // Start with an empty string.
     uint64_t y = 10000000000000000000ULL;
     int res;
     if (x == 0)  // if x = 0 and this is not testet, then function return a empty string.
     {
           str = "0";
           return str;  // or return "0";
     }    
     while (y > 0)
     {                
            res = (int)(x / y);
            if (res > 0)  // Wait for res > 0, then start adding to string.
                flag = true;
            if (flag == true)
                str = str + String(res);
            x = x - (y * (uint64_t)res);  // Subtract res times * y from x
            y = y / 10;                   // Reducer y with 10    
     }
     return str;
} 

/*
 * Print out the data stored in I2CDataArray global
 */
void I2CMaster_PrintData(void)
{
  for (int e = 0; e < BUFFER_ENTRIES; e++)                                  
  {
    //note that print does not support uint64_t
    Serial.print(ToString(I2CDataArray[e].data));
    (e == (BUFFER_ENTRIES - 1)) ? Serial.println() : Serial.print(",");
  }
}

/*
 * Read and return unsigned int of specified size from the byte array.
 */
uint64_t I2CMaster_UnpackBytesToInt(const uint8_t byteArray[], const uint8_t numBytes)
{ 
  uint64_t data = 0;
  
  for (uint8_t i = 0; i < numBytes; i++)
  {
    data |= (uint64_t)byteArray[i] << (8 * i);
  }
  return data;
}

/*
 * Enter data into the .idx field of I2C buffer struct. Save the location of 
 * the start of each entry in the I2CByteBuffer 
*/
void I2CMaster_WriteByteMapping(data_t dataArray[], const uint8_t nEntries)
{
  uint8_t entryIdx = 0;
  for (int i = 0; i < nEntries; i++)
  {
    dataArray[i].idx = entryIdx;
    entryIdx += dataArray[i].numBytes;
  }
}

/*
 * Count the number of bytes to read
 */
uint8_t I2CMaster_CountBytes(const data_t dataArray[], const uint8_t nEntries)
{
  uint8_t totalBytes = 0;

  for (int i = 0; i < nEntries; i++)
  {
    totalBytes += dataArray[i].numBytes;
  }
  return totalBytes;
}

/*
 * Read contents of entire byte buffer and write data to the data array. Each
 * entry is read individually and passed to the dataArray struct.
 */
void I2CMaster_ReadByteBuffer(uint8_t byteArray[], data_t dataArray[], const uint8_t nEntries)
{
  for (int i = 0; i < nEntries; i++)
  {
    dataArray[i].data = I2CMaster_UnpackBytesToInt((byteArray + dataArray[i].idx), dataArray[i].numBytes);
  }
}

/*
 * Read bytes over the I2C and pass them into the byte buffer.
 */
void I2CMaster_ReadBytes(uint8_t I2CByteBuffer[], const uint8_t slaveAddress, const uint8_t nBytes)
{
  uint16_t bytesAvailable;
  
  Wire.beginTransmission(slaveAddress);
  Wire.requestFrom(slaveAddress, nBytes);
  if (nBytes == Wire.available())
  {
    for(int i = 0; i < nBytes; i++)
    {
      I2CByteBuffer[i] = Wire.read();
    }
  }
  else
  {
    Serial.print("I2C read error. ");
    Serial.print(bytesAvailable);
    Serial.print(" bytes available. ");
    Serial.print(nBytes);
    Serial.println(" requested.");
  }
  //TODO: endtransmission returns an error code. Error check on this.
  Wire.endTransmission(slaveAddress);
}

/*
 * Function that scans the I2C bus. If a device is found, the result is printed to
 * the serial window.
 */
void I2CMaster_Scanner(void)
{
  uint8_t countDevices;
  uint8_t errorCode;
  //addresses 0 to 7, and 120 to 127 are reserved  
  for (byte i = 8; i <= 119; i++)
  {
    Wire.beginTransmission(i);
    delay (1);  // this is required
    errorCode = Wire.endTransmission();
    //no error so report address.
    if (errorCode == 0)
    {
      Serial.print("Found address: ");
      Serial.print(i, DEC);
      Serial.println(" No Error.");
      countDevices++;
    }
    //Error condition. Report it here.
    else if (errorCode == 4)
    {
      Serial.print("Found address: ");
      Serial.print(i, DEC);
      Serial.println(" Unkown error.");
      countDevices++;
    }
    else
    {
      //move on to next one
    }
  } 
  Serial.println ("Done.");
  Serial.print ("Found ");
  Serial.print (countDevices, DEC);
  Serial.println (" device(s).");
}

//read serial data if there is any and pass to inputString global
void serialEvent(void)
{
  if (Serial.available() > 0)
  {
    inputString = Serial.readString();
  }
}

void setup()
{
  inputString = "";
  bytesToRead = I2CMaster_CountBytes(I2CDataArray, BUFFER_ENTRIES);
  
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  //Serial.println("timer, V_a, I_a, V_b, I_b, Temp, Intensity, err_a, err_b");
  I2CMaster_WriteByteMapping(I2CDataArray, BUFFER_ENTRIES);
}

void loop()
{
  uint8_t parsedAddress;
  //Note that commands must not contain a carriage return. Turn this option to no line ending in the serial window.
  if (inputString.length() > 0)
    {
    switch (inputString.charAt(0))
    {
      case ('S'):
        Serial.println("Scanning for I2C connections...");
        I2CMaster_Scanner();
        break;
      case ('R'):
        parsedAddress = inputString.substring(1).toInt();
        if (parsedAddress <=119 && parsedAddress >= 8)
        {
          Serial.print("Reading data from I2C address ");
          Serial.println(parsedAddress);
          BUFFER_RST();
          BUFFER_READ(parsedAddress);
          I2CMaster_PrintData();
        }
        else
        {
          Serial.println("Invalid I2C address. Value expected between 8 and 119.");
        }
        break;
      case ('B'):
        Serial.println("Printing contents of I2C buffer...");
        BUFFER_PRINT();
        break;
      default:
        Serial.println("Command not recognised.");
        break;
    }
  }
  inputString = "";
  delay(tDelay);
  
}
