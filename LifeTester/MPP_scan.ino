/*
function to find MPP by scanning over voltages. Updates v to the MPP. 
also pass in objects referring to the led, DAC and ADC for this channel.
*/
void MPP_scan(lifetester *channel_x, int startV, int finV, int dV, MCP4822 &DAC)
{
  unsigned long v;
  unsigned long vMPP;   //everything needs to be defined as long to calculate power correctly
  unsigned long p_max = 0;
  unsigned long p_scan;
  unsigned long i_scan = 0;
  unsigned long i_max;
  unsigned long mytimer = millis();
  
  int nSamples = 0; //number of readings taken during sample time

  #if DEBUG
    Serial.println("starting measurement");
    Serial.println("V, I, P, error, channel");
  #endif
  
  channel_x->Led.t(25, 500);
 
  //scan voltage and look for max power point
  v = startV;
  
  while(v <= finV)
  {
    unsigned long t_elapsed = millis() - mytimer;
    channel_x->Led.update();
    channel_x->iv_data.v = v;
    
    //measurement speed defined by settle time and sample time
    if (t_elapsed < settle_time)
    {
      //STAGE 1 (DURING SETTLE TIME): SET TO VOLTAGE 
      DAC.output(channel_x->DAC_channel,v);    
      DAC_errmsg = DAC.readErrmsg();
      i_scan = 0;
    }
      
    else if ((t_elapsed >= settle_time) && (t_elapsed < (settle_time + sampling_time)))
    {  
      //STAGE 2: (DURING SAMPLING TIME): KEEP READING ADC AND STORING DATA.
      i_scan += channel_x->ADC_input.readInputSingleShot();
      nSamples++;
      channel_x->iv_data.i_transmit = i_scan / nSamples; //i_current is requested by I2C
    }

    else if (t_elapsed >= (settle_time + sampling_time))
    {
      //STAGE 3: MEASUREMENTS FINISHED. UPDATE MAX POWER IF THERE IS ONE. RESET VARIABLES.
      i_scan /= nSamples;
      nSamples = 0;
      p_scan = i_scan * v;

      //update max power and vMPP if we have found a maximum power point.
      if (p_scan > p_max)
      {  
        p_max = i_scan * v;
        i_max = i_scan;
        vMPP = v;
      }  

      if (i_scan >= 4095)
        channel_x->error = current_limit;  //reached current limit
      if (DAC_errmsg != 0) //DAC writing error
        channel_x->error = DAC_error;

      #if DEBUG
        Serial.print(v);
        Serial.print(", ");
        Serial.print(i_scan);
        Serial.print(", ");
        Serial.print(p_scan);
        Serial.print(", ");
        Serial.print(channel_x->error);
        Serial.print(", ");
        Serial.println(channel_x->DAC_channel);
      #endif
      
      mytimer = millis(); //reset timer
      
      v += dV;  //move to the next point
    }
  }

  #if DEBUG
    Serial.println();
    Serial.print("i_max = ");
    Serial.print(i_max);
    Serial.print(", Vmpp = ");
    Serial.print(vMPP);
    Serial.print(", error = ");
    Serial.println(channel_x->error, DEC);
  #endif
  
  if (i_max < i_threshold)
    channel_x->error = threshold;  

  channel_x->iv_data.v = vMPP;
  //reset DAC
  DAC.output(channel_x->DAC_channel, 0);    
}
