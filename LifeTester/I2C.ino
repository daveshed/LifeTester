/*
function that executes whenever data is requested by master
this function is registered as an event, see setup()
all other code is interrupted and the data sent to master 
send data as a string of 14 bytes...

MSB/LSB(v_a) MSB/LSB(i_a) MSB/LSB(v_b) MSB/LSB(i_b)
MSB/LSB(T) MSB/LSB(intensity)error_A error B

TO DO: buffer readings and transmit everything using termination byte.
*/

void request_event(void)
{
  Wire.write(I2C_data, 14);  
}

void I2C_prepare_data(byte data[])
{
  unsigned int  Temp = T_sense.raw_data,
                Intensity = analogRead(LDR_pin);
  
  //convert unsigned ints into two byte strings
  data[0] = channel_A.iv_data.v >> 8;
  data[1] = channel_A.iv_data.v;

  data[2] = channel_A.iv_data.i_transmit >> 8;
  data[3] = channel_A.iv_data.i_transmit;

  data[4] = channel_B.iv_data.v >> 8;
  data[5] = channel_B.iv_data.v;

  data[6] = channel_B.iv_data.i_transmit >> 8;
  data[7] = channel_B.iv_data.i_transmit;

  data[8] = Temp >> 8;
  data[9] = Temp;
  
  data[10] = Intensity >> 8;
  data[11] = Intensity;

  data[12] = channel_A.error;
  data[13] = channel_B.error;
}

/*
//helper function to split data into two byte strings and send over I2C interface to master
void I2C_transmit(unsigned int data)
{
  byte MSB,LSB;
  
  MSB = data >> 8;
  LSB = data >> 0;
  Wire.write(MSB);
  Wire.write(LSB);
}
*/

