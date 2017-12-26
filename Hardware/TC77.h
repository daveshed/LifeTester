#ifndef TC77_H
#define TC77_H
#ifdef _cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <stdbool.h>

void TC77_Init(uint8_t chipSelectPin);
float TC77_ConvertToTemp(uint16_t rawData);
void TC77_Update(void);
uint16_t TC77_GetRawData(void);
bool TC77_GetError(void);

void ResetTimer(void);

#ifdef _cplusplus
}
#endif

#endif