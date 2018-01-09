#ifndef TESTUTILS_H
#define TESTUTILS_H

#include "MockArduino.h"  // pinState_t
#include <stdbool.h>
#include <stdint.h>

/*
 Accesses mock digital pins defined in MockArduino.c. Checks that the mock
 digital pin matches input args.
 */
void CheckMockPinOutputState(int pinNum, bool expectedOn);

/* 
 Reinitialises mock digital pins to unassigned ie neither input or output
 specified. Output set to off.
 */
void ResetDigitalPins(pinState_t *pins, uint8_t nPins);

#endif // TESTUTILS_H