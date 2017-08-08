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

#define MAX_DATA_SIZE       4 //4 bytes for a uint64_t
#define BUFFER_ENTRIES      9
#define BUFFER_MAX_SIZE     MAX_DATA_SIZE * BUFFER_ENTRIES

const uint8_t   slaveAddress = 8; //address
const uint16_t  tDelay = 1000;
uint8_t         I2CByteBuffer[BUFFER_MAX_SIZE] = {0};
uint8_t         bytesToRead;
uint8_t         writeIdx = 0;

/*
 * Macro to read an unsigened int from the buffer
 * at the location specified by index, idx.
 * variable returned as uint64_t buffer array of uint8_t ref needed
 */
#define BUFFER_READ()                                                   \
  I2CMaster_ReadBytes(I2CByteBuffer, slaveAddress, bytesToRead);        \
  I2CMaster_ReadByteBuffer(I2CByteBuffer, I2CDataArray, BUFFER_ENTRIES)


/*
 * Macro which clears the bytebuffer and resets the index
*/
#define BUFFER_RST()                                    \
  I2CMaster_ClearArray(I2CByteBuffer, BUFFER_MAX_SIZE); \
  writeIdx = 0                

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
void I2CMaster_ClearArray(uint8_t byteArray[], const uint8_t numBytes)
{
  for (int i = 0; i < numBytes; i++)
  {
    byteArray[i] = 0;
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
 * Read and return unsigned int of specified size from the byte array.
 */
uint64_t I2CMaster_UnpackBytesToInt(const uint8_t byteArray[], const uint8_t numBytes)
{ 
  uint64_t data = 0;
  for (int i = 0; i < numBytes; i++)
  {
    data |= byteArray[i] << (8 * i);
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

void I2CMaster_ReadBytes(uint8_t I2CByteBuffer[], uint8_t slaveAddress, uint8_t nBytes)
{
  Wire.beginTransmission(slaveAddress);
  Wire.requestFrom(slaveAddress, nBytes);

  for(int i = 0; i < nBytes; i++)
  {
    I2CByteBuffer[i] = Wire.read();
  }
  
  Wire.endTransmission(slaveAddress);
}

void setup()
{
  bytesToRead = I2CMaster_CountBytes(I2CDataArray, BUFFER_ENTRIES);
  I2CMaster_WriteByteMapping(I2CDataArray, BUFFER_ENTRIES);
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  Serial.println("timer, V_a, I_a, V_b, I_b, Temp, Intensity, err_a, err_b");
}

void loop()
{
  BUFFER_RST();
  BUFFER_READ();
  I2CMaster_PrintByteArray(I2CByteBuffer, bytesToRead);
  
  // read out elements one at a time to test this
  for (int e = 0; e < BUFFER_ENTRIES; e++)
  {
    //note that print does not support uint64_t
    Serial.print((uint32_t)I2CDataArray[e].data);
    (e != (BUFFER_ENTRIES - 1)) ? Serial.print(", ") : Serial.println();
  } 
  delay(tDelay);
}
