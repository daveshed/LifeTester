#include "Macros.h"

#define BUFFER_MAX_SIZE   (32U)

typedef struct DataBuffer_s {
    uint8_t d[BUFFER_MAX_SIZE];
    uint8_t tail;
    uint8_t head;
} DataBuffer_t;

#ifdef UNIT_TEST
    extern DataBuffer_t transmitBuffer;
    extern DataBuffer_t receiveBuffer;
#endif

STATIC void ResetBuffer(DataBuffer_t *const buf);
static bool IsFull(DataBuffer_t const *const buf);
STATIC void WriteUint8(DataBuffer_t *const buf, uint8_t data);
static void WriteUint16(DataBuffer_t *const buf, uint16_t data);
static void WriteUint32(DataBuffer_t *const buf, uint32_t data);
STATIC uint8_t CheckSum(DataBuffer_t const *const buf);
STATIC void PrintBuffer(DataBuffer_t const *const buf);