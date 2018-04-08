#ifndef STATEMACHINE_H
#define STATEMACHINE_H
#include <stdint.h>
#include "LifeTesterTypes.h"

void StateMachine_Reset(LifeTester_t *const lifeTester);

void StateMachine_UpdateStep(LifeTester_t *const lifeTester);

#endif