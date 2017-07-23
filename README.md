# LifeTester
Solar cell maximum power point tracking system based on Arduino

ALGORITHM
Routine to track MPP by hill climbing method:
1) Connect devices (two channels: A and B) and power up/press rst button
2) Initial voltage sweeps are carried out first to determine an initial guess for MPP
3) Devices then set to initial MPP guess and tracking continues indefinitely
4) If an error is raised, that channel will remain in error state until the Lifetester is reset

CS PIN ASSIGNMENTS
7 ADS1286   ADC A
8 ADS1286   ADC B //not available
9 MAX6675   Thermocouple reading  //not available
10 MCP4822  DAC

DATA TRANSMITTED VIA I2C
Connect pins 4/5 to pins 4/5 of master (SDA/SCL pins)
Up to 128 devices can be connected to the same bus and called by address no.
TO DO: set address using digitals and jumpers
see: https://www.arduino.cc/en/Tutorial/MasterReader
https://electronics.stackexchange.com/questions/25278/how-to-connect-multiple-i2c-interface-devices-into-a-single-pin-a4-sda-and-a5

SUMMARY OF ERROR CODES (1 byte)
0 - no error
1 - low current/open circuit
2 - reached current limit
3 - below test threshold current
4 - DAC writing error/out of range
*/
