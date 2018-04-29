#ifndef CONTROLLER_H
#define CONTROLLER_H

#ifdef _cplusplus
extern "C" {
#endif

#include "LifeTesterTypes.h"

void Controller_Init(void);
void Controller_ConsumeCommand(LifeTester_t *const lifeTesterChA,
                               LifeTester_t *const lifeTesterChB);
void Controller_ReceiveHandler(int numBytes);
void Controller_RequestHandler(void);


#ifdef _cplusplus
}
#endif

#endif //include guard