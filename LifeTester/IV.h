#ifndef IV_H
#define IV_H
#include <stdint.h>
#include "LifeTesterTypes.h"

void IV_ScanAndUpdate(LifeTester_t * const lifeTester,
                      const uint16_t startV,
                      const uint16_t finV,
                      const uint16_t dV);
void IV_MpptUpdate(LifeTester_t * const lifeTester);

#endif