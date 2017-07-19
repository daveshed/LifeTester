/*
250317 D. Mohamad
Routine to track MPP by hill climbing method
1)ensure device is covered initially so we can measure the offset voltage and subtract
2)then we run an IV sweep to find the MPP initial guess
3)maintain optimum power output by hill climbing

120417 I2C version 0c - allow some tolerance before throwing up an error.
(a) several low/high current readings allowed before error
(b) wider band of allowed currents before test starts
version 0d - ADC library altered: dummy reading first. average over n readings. 
version 0e - issue with MPPT. keep measuring until next time interval. only want to run each measurement once to stop v in/decrementing.
version 2a - built new board with offset correction and precise gain setting - range is set with series resistor. no need for offset stage
version 2b - tested board and code with a torch. MPP tracking working for increasing and decreasing light intensity.
version 2c - ADC averaging fixed. Noise issue relates to changing power ourput because voltage is being changed continuously - MPPT!.
version 2d - Amended MPPscan function to use settle time and sample time approach. ADC sampled continuously during sample time.
version 2e - tested working better. Less noise - DAC not changing voltage properly. Removed DACset variable and if statement.
version 3a - 240417 new board with two channels and TI quad op amp.
version 3b - attempted to make lifetime tester class. couldn't do it.
version 3c - monitor light intensity using LDR on A0.

CS PIN ASSIGNMENTS
7 ADS1286   ADC A
8 ADS1286   ADC B //not available
9 MAX6675   Thermocouple reading  //not available
10 MCP4822  DAC

DATA TRANSMITTED VIA I2C
connect pins 4/5 to pins 4/5 of master (SDA/SCL pins)
lots (how many?) devices can be connected to the same bus and called by address no.
TO DO: set address using digitals and jumpers
see: https://www.arduino.cc/en/Tutorial/MasterReader

SUMMARY OF ERROR CODES (1 byte)
0 - no error
1 - low current/open circuit
2 - reached current limit
3 - below test threshold current
4 - DAC writing error/out of range
*/
