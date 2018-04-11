#define MAX_DATA_SIZE     8 //8 bytes for a uint64_t
#define BUFFER_ENTRIES    9
#define BUFFER_MAX_SIZE   MAX_DATA_SIZE * BUFFER_ENTRIES

#ifdef UNIT_TEST
    extern uint8_t I2CByteBuffer[BUFFER_MAX_SIZE];
    extern uint8_t bufferIdx;
#endif