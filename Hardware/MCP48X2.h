#ifndef MCP48X2_H
#define MCP48X2_H

#ifdef _cplusplus
extern "C"
{
#endif

#include <stdint.h>

typedef enum gainSelect_e {
    highGain,
    lowGain
} gainSelect_t;

typedef enum shdnSelect_e {
    shdnOn,
    shdnOff
} shdnSelect_t;

typedef enum chSelect_e {
    chASelect,
    chBSelect
} chSelect_t;

void MCP48X2_Init(uint8_t pin);
void MCP48X2_Output(uint8_t output, char channel);
void MCP48X2_SetGain(char requestedGain);
char MCP48X2_GetGain(void);
uint8_t MCP48X2_GetErrmsg(void);

#ifdef _cplusplus
}
#endif

#endif