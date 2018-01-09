#ifndef TC77_H
#define TC77_H
#ifdef _cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <stdbool.h>

// Initialises chip select pin and static variables
void TC77_Init(uint8_t chipSelectPin);

// Converts rawData from the temperature controller to temperature in deg C
float TC77_ConvertToTemp(uint16_t rawData);

// Updates measurement if it's time and checks for error condition.
void TC77_Update(void);

// Gets raw data from the spi read register
uint16_t TC77_GetRawData(void);

// Gets the current error condition
bool TC77_GetError(void);

#ifdef _cplusplus
}
#endif

#endif