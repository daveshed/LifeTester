/*
D. Mohamad 12/04/17
sketch to be uploaded to lifetime testing master device
for reading data from the slave test board
data received as a string of 9 bytes:
MSB//LSB(v) MSB/LSB(i) MSB/LSB(T) MSB/LSB(light intensity) error
voltage first
*/

#include <Wire.h>

const int bytesToRead = 14; //bytes to read
const int slaveAddress = 8; //address
const unsigned int t_delay = 1000;

unsigned int I2CReadData(void);

void setup()
{
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  Serial.println("V_a, I_a, V_b, I_b, Temp, Intensity, err_a, err_b");
}

void loop() {
  Wire.beginTransmission(slaveAddress);
  byte buf[14];  
  int n = Wire.requestFrom(slaveAddress, bytesToRead);

  for( int i = 0; i < n; i++)
  {
    buf[i] = Wire.read();
  }
  
  Wire.endTransmission(slaveAddress);
  
  Serial.print((buf[0] << 8) | buf[1]);
  Serial.print(", ");
  Serial.print((buf[2] << 8) | buf[3]);
  Serial.print(", ");
  Serial.print((buf[4] << 8) | buf[5]);
  Serial.print(", ");
  Serial.print((buf[6] << 8) | buf[7]);
  Serial.print(", ");
  Serial.print((buf[8] << 8) | buf[9]);
  Serial.print(", ");
  Serial.print((buf[10] << 8) | buf[11]);
  Serial.print(", ");
  Serial.print(buf[12]);
  Serial.print(", ");
  Serial.print(buf[13]);
  Serial.println();
  
  delay(t_delay);
}

/*
//helper function to read two bytes from the slave device and return as int
//note that all data is 12bits or narrower and are each transmitted as two bytes
//apart from error which is one byte wide - don't use this function for error
unsigned int I2C_receive_int(void)
{
  byte MSB,LSB;
  unsigned int data=0;
  
  MSB = Wire.read();
  LSB = Wire.read();  

  data = (MSB<<8) | LSB;
  return(data);
}
*/
