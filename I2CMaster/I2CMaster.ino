/*
sketch to be uploaded to lifetime testing master device
for reading data from the slave test board
data received as a string of 17 bytes:
time MSB//LSB(v) MSB/LSB(i) MSB/LSB(T) MSB/LSB(light intensity) error
voltage first
*/

#include <Wire.h>

#define BYTES_TO_READ 17
const int slaveAddress = 8; //address
const unsigned int tDelay = 1000;
uint8_t I2CBuffer[BYTES_TO_READ];  
unsigned int I2CReadData(void);

void setup()
{
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  Serial.println("time, V_a, I_a, V_b, I_b, Temp, Intensity, err_a, err_b");
}

void loop()
{
  I2CReadData(I2CBuffer, slaveAddress, BYTES_TO_READ);
  
  Serial.print(unpack(I2CBuffer, 0, 4));
  Serial.print(", ");
  for (int i = 0; i < 6; i++)
  {
    Serial.print(unpack(I2CBuffer, 4 + i * 2, 2));
    Serial.print(", ");
  }
  Serial.print(I2CBuffer[15]);
  Serial.print(", ");
  Serial.print(I2CBuffer[15]);
  Serial.println();
  
  delay(tDelay);
}

void I2CReadData(uint8_t buf[], uint8_t slaveAddress, uint8_t nBytes)
{
  Wire.beginTransmission(slaveAddress);
  Wire.requestFrom(slaveAddress, nBytes);

  for(int i = 0; i < nBytes; i++)
  {
    buf[i] = Wire.read();
  }
  
  Wire.endTransmission(slaveAddress);
}
/*
Helper function to read specified number of bytes from buffer byte array.
Returns an int containing data. NB. MSB is at lowest index
*/
uint32_t unpack(const uint8_t byteArray[], uint8_t index, uint8_t numBytes)
{
  uint32_t  data;
  uint8_t   i;
  uint8_t   j;
  data = 0;
  
  for (i = index, j = numBytes - 1; i < index + numBytes; i++, j--)
  {
     data = byteArray[i] << (j * 8);
  }
  
  return(data);
}
