/*
 Module responsible for implementation of the I2C controller. Commands are 
 written into a byte wide register which is updated so that a read from this
 register will notify status eg. ready or error.
 register map
 Bit:  7  6   5  4  3  2  1  0
 Func: Ch RW RDY X  X |  CMD  |
 comms register mask and bit shifts
*/
#ifndef CONTROLLER_H
#define CONTROLLER_H

#ifdef _cplusplus
extern "C" {
#endif

#include "LifeTesterTypes.h"

/*
 Initialises controller register and clears transmit buffer
*/
void Controller_Init(void);

/*
 Updates the sate of the controller after a command has been received. This 
 function is called repeatedly in the main loop.
*/
void Controller_ConsumeCommand(LifeTester_t *const lifeTesterChA,
                               LifeTester_t *const lifeTesterChB);

/*
 Handles a data write from the master device over I2C. Slave (LifeTestere)
 expects either a new command or a write to the params register.
*/
void Controller_ReceiveHandler(int numBytes);

/*
 Handles a request for data from the master which may be either a read from the
 (buffered) data or params registers or simply a read from the command register
 to check status or error etc.
*/
void Controller_RequestHandler(void);


#ifdef _cplusplus
}
#endif

#endif //include guard