#include "Arduino.h"
#include "Config.h"
#include "I2C.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include <stdint.h>
#include "Wire.h"

#define MAX_DATA_SIZE     8 //8 bytes for a uint64_t
#define BUFFER_ENTRIES    9
#define BUFFER_MAX_SIZE   MAX_DATA_SIZE * BUFFER_ENTRIES

/*
function that executes whenever data is requested by master
this function is registered as an event, see setup()
all other code is interrupted and the data sent to master 
send data as a sequence of 18 bytes...

timer (uint32_t) MSB/LSB(v_a) MSB/LSB(i_a) MSB/LSB(v_b) MSB/LSB(i_b)
MSB/LSB(T) MSB/LSB(intensity)error_A error B
*/

uint8_t I2CByteBuffer[BUFFER_MAX_SIZE] = {0};
uint8_t bufferIdx = 0;

/*
 * Macro to write an unsigened int to a the buffer
 * Data can take 8, 16, 32 or 64 bit size and 
 * should be passed by value. Buffer idx global and incremented
 * according to data size.
 */
#define BUFFER_WRITE(data)                                            \
  I2C_PackIntToBytes(data, (I2CByteBuffer + bufferIdx), sizeof(data));\
  bufferIdx += sizeof(data)

#define BUFFER_RST()                              \
  I2C_ClearArray(I2CByteBuffer, BUFFER_MAX_SIZE); \
  bufferIdx = 0

void I2C_ClearArray(uint8_t byteArray[], const uint8_t numBytes)
{
  for (int i = 0; i < numBytes; i++)
  {
    byteArray[i] = 0;
  }
}

/*
 * packing an unsigned int into individual bytes and copying into the array arg
 * LSB will be at the lower index ie. for a uint16_t byteArray[i] = LSB, [i+1] = MSB
 */
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
  Wire.write(I2CByteBuffer, bufferIdx);  
}

void I2C_PrepareData(LifeTester_t const *const LTChannelA, LifeTester_t const *const LTChannelB)
{
  BUFFER_RST();
  
  // assign elements of the buffer
  
  BUFFER_WRITE(millis());
  BUFFER_WRITE((uint16_t)*LTChannelA->data.vActive);
  BUFFER_WRITE((uint16_t)*LTChannelA->data.iActive);
  BUFFER_WRITE((uint16_t)*LTChannelB->data.vActive);
  BUFFER_WRITE((uint16_t)*LTChannelB->data.iActive);
  BUFFER_WRITE((uint16_t)TempGetRawData());
  BUFFER_WRITE((uint16_t)analogRead(LIGHT_SENSOR_PIN));
  BUFFER_WRITE((uint8_t)LTChannelA->error);
  BUFFER_WRITE((uint8_t)LTChannelB->error);

}
