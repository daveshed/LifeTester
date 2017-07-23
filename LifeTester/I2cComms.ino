/*
function that executes whenever data is requested by master
this function is registered as an event, see setup()
all other code is interrupted and the data sent to master 
send data as a sequence of 14 bytes...

MSB/LSB(v_a) MSB/LSB(i_a) MSB/LSB(v_b) MSB/LSB(i_b)
MSB/LSB(T) MSB/LSB(intensity)error_A error B

TO DO: buffer readings and transmit everything using termination byte.
*/

void I2C_TransmitData(void)
{
  Wire.write(I2CData, 14);  
}

void I2C_PrepareData(uint8_t data[])
{
  uint16_t Temp = TSense.raw_data;
  uint16_t Intensity = analogRead(LdrPin);
  
  //convert unsigned ints into two byte strings
  data[0] = LTChannelA.IVData.v >> 8;
  data[1] = LTChannelA.IVData.v;

  data[2] = LTChannelA.IVData.iTransmit >> 8;
  data[3] = LTChannelA.IVData.iTransmit;

  data[4] = LTChannelB.IVData.v >> 8;
  data[5] = LTChannelB.IVData.v;

  data[6] = LTChannelB.IVData.iTransmit >> 8;
  data[7] = LTChannelB.IVData.iTransmit;

  data[8] = Temp >> 8;
  data[9] = Temp;
  
  data[10] = Intensity >> 8;
  data[11] = Intensity;

  data[12] = LTChannelA.error;
  data[13] = LTChannelB.error;
}