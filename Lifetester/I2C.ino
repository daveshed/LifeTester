#include "I2C.h"

/*
function that executes whenever data is requested by master
this function is registered as an event, see setup()
all other code is interrupted and the data sent to master 
send data as a sequence of 18 bytes...

timer (uint32_t) MSB/LSB(v_a) MSB/LSB(i_a) MSB/LSB(v_b) MSB/LSB(i_b)
MSB/LSB(T) MSB/LSB(intensity)error_A error B
*/

//Global variables declared in I2C.h. Defined here...
uint8_t I2CByteBuffer[BUFFER_MAX_SIZE] = {0};
uint8_t bufferIdx = 0;

/*
 * Macro to write an unsigened int to a the buffer
 * Data can take 8, 16, 32 or 64 bit size and 
 * should be passed by value. Buffer idx global and incremented
 * according to data size.
 */
#define BUFFER_WRITE(data)                                        \
  I2C_PackIntToBytes(data, (I2CByteBuffer + bufferIdx), sizeof(data));\
  bufferIdx += sizeof(data)

#define BUFFER_RST()                          \
  I2C_ClearArray(I2CByteBuffer, BUFFER_MAX_SIZE); \
  bufferIdx = 0

void I2C_ClearArray(uint8_t byteArray[], const uint8_t numBytes)
{
  for (int i = 0; i < numBytes; i++)
  {
    byteArray[i] = 0;
  }
}

void I2C_PackIntToBytes(const uint64_t data, uint8_t byteArray[], const uint8_t numBytes)
{
  //TO DO: assert statement to check the size of data does not exceed uint64_t

  for (int i = 0; i < numBytes; i++)
  {
    byteArray[i] = data >> (8 * i);
  }
}

void I2C_PrintByteArray(const uint8_t byteArray[], const uint16_t n)
{
  Serial.print("byteArray[] =");
  for (int i = 0; i < n; i++)
  {
    Serial.print(" ");
    Serial.print(byteArray[i]);
  }
  Serial.println();
}

void I2C_TransmitData(void)
{
  Wire.write(I2CByteBuffer, BUFFER_MAX_SIZE);  
}

void I2C_PrepareData(void)
{
  BUFFER_RST();

  uint16_t  Temp = TSense.raw_data;
  uint16_t  Intensity = analogRead(LdrPin);
  uint32_t  tTransmit = 0.5 * (LTChannelA.timer + LTChannelB.timer);
  
  //declare array to store variables that we want to transmit
  uint32_t  I2CDataToTransmit[BUFFER_ENTRIES] =
  {
    tTransmit,
    LTChannelA.IVData.v,
    LTChannelA.IVData.iTransmit,
    LTChannelB.IVData.v,
    LTChannelB.IVData.iTransmit,
    Temp,
    Intensity,
    LTChannelA.error,
    LTChannelB.error
  };

  // assign elements of the buffer
  for (int i = 0; i < BUFFER_ENTRIES; i++)
  {
    BUFFER_WRITE(I2CDataToTransmit[i]);
  }
}
