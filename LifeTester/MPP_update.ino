/*
 * function to track/update maximum point durint operation
 * Pass in lifetester struct and values are updated
 * only track if we don't have an error and it's time to measure
 * error 3 means that the current was too low to start the measurement (in MPPscan)
 * allow a certain number of error readings before triggering error state/stop measurement
 * start measuring after the tracking delay
 */

void MPP_update(lifetester *channel_x, MCP4822 &DAC)
{
  unsigned long t_elapsed = millis() - channel_x->timer;
  
  if ((channel_x->error != threshold) && (channel_x->n_error_reads < tolerance))
  {
    
    if ((t_elapsed >= tracking_delay) && t_elapsed < (tracking_delay + settle_time))
    {
      //STAGE 1: SET INITIAL STATE OF DAC V0
      DAC.output(channel_x->DAC_channel, channel_x->iv_data.v); //v is global
    }
    
    else if ((t_elapsed >= (tracking_delay + settle_time)) && (t_elapsed < (tracking_delay + settle_time + sampling_time)))  
    {
      //STAGE 2: KEEP READING THE CURRENT AND SUMMING IT AFTER THE SETTLE TIME
      channel_x->iv_data.i_cur += channel_x->ADC_input.readInputSingleShot();
      channel_x->n_reads_cur++;
    }
    
    else if (t_elapsed >= (tracking_delay + settle_time + sampling_time) && (t_elapsed < (tracking_delay + 2 * settle_time + sampling_time)))
    {
      //STAGE 3: STOP SAMPLING. SET DAC TO V1.
      DAC.output(channel_x->DAC_channel, (channel_x->iv_data.v + dV_MPPT));
    }
    
    else if (t_elapsed >= (tracking_delay + 2*settle_time + sampling_time) && (t_elapsed < (tracking_delay + 2 * settle_time + 2 * sampling_time)))
    {
      //STAGE 4: KEEP READING THE CURRENT AND SUMMING IT AFTER ANOTHER SETTLE TIME
      channel_x->iv_data.i_next += channel_x->ADC_input.readInputSingleShot();
      channel_x->n_reads_next++;
    }
    
    else if (t_elapsed >= (tracking_delay + 2 * settle_time + 2 * sampling_time))
    {
      //STAGE 5: MEASUREMENTS DONE. DO CALCULATIONS
      channel_x->iv_data.i_cur /= channel_x->n_reads_cur; //calculate average
      channel_x->iv_data.p_cur = channel_x->iv_data.v * channel_x->iv_data.i_cur; //calculate power now
      channel_x->n_reads_cur = 0; //reset counter

      channel_x->iv_data.i_next /= channel_x->n_reads_next;
      channel_x->iv_data.p_next = (channel_x->iv_data.v + dV_MPPT) * channel_x->iv_data.i_next;
      channel_x->n_reads_next = 0;

      //if power is lower here, we must be going downhill then move back one point for next loop
      if (channel_x->iv_data.p_next > channel_x->iv_data.p_cur)
      {
        channel_x->iv_data.v += dV_MPPT;
        channel_x->Led.stopAfter(2); //two flashes
      }
      else
      {
        channel_x->iv_data.v -= dV_MPPT;
        channel_x->Led.stopAfter(1); //one flash
      }
  
      DAC_errmsg = DAC.readErrmsg();
            
      //finished measurement now so do error detection
      if (channel_x->iv_data.p_cur == 0)
      {
        channel_x->error = low_current;  //low power error
        channel_x->n_error_reads++;
      }
      else if (channel_x->iv_data.i_cur >= 4095)
      {
        channel_x->error = current_limit;  //reached current limit
        channel_x->n_error_reads++;
      }
      else if (DAC_errmsg != 0)
      {
        channel_x->error = DAC_error;  //DAC writing error
        channel_x->n_error_reads++;
      }
      else//no error here so reset error counter and err_code to 0
      {
        channel_x->error = 0;
        channel_x->n_error_reads = 0;
      }

      #if DEBUG
        Serial.print(channel_x->iv_data.v);
        Serial.print(", ");
        Serial.print(channel_x->iv_data.i_cur);
        Serial.print(", ");
        Serial.print(channel_x->iv_data.p_cur);
        Serial.print(", ");
        Serial.print(channel_x->error,DEC);
        Serial.print(", ");
        Serial.print(analogRead(LDR_pin));
        Serial.print(", ");
        Serial.print(T_sense.T_deg_C);
        Serial.print(", ");
        Serial.print(channel_x->DAC_channel);
        Serial.println();
      #endif

      channel_x->iv_data.i_transmit =
        0.5 * (channel_x->iv_data.i_cur + channel_x->iv_data.i_next);
      channel_x->timer = millis(); //reset timer
      channel_x->iv_data.i_cur = 0;
      channel_x->iv_data.i_next = 0;
    }
  }
  
  else //error condition - trigger LED
  {
    //error condition
    channel_x->Led.t(500,500);
    channel_x->Led.keepFlashing();
  }
}
