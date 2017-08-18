
void MCP48X2_Init(uint8_t chipSelectPin);
void MCP48X2_Output(char chipSelectPin, uint8_t output, char channel);
void MCP48X2_SetGain(char requestedGain);
char MCP48X2_GetGain(void);
uint8_t MCP48X2_GetErrmsg(void);

