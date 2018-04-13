#ifndef CONTROLLER_H
#define CONTROLLER_H

#ifdef _cplusplus
extern "C" {
#endif

#include "LifeTesterTypes.h"


void Controller_TransmitData(void);
/*
function that executes whenever data is requested by master
this function is registered as an event, see setup()
all other code is interrupted and the data sent to master 
send data as a sequence of 17 bytes...

Variable   n Bytes
timer      4
v_a        1
i_a        2
v_b        1
i_b        2
T          2
I_light    2
error_A    1
error B    1
Checksum   1
TOTAL -> 17 Bytes
*/
void Controller_WriteDataToBuffer(LifeTester_t const *const LTChannelA,
                                  LifeTester_t const *const LTChannelB);

/*
Api to implement


*/


#ifdef _cplusplus
}
#endif

#endif //include guard