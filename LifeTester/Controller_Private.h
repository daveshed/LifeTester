#include "Macros.h"

#define BUFFER_MAX_SIZE   (32U)

typedef struct DataBuffer_s {
    uint8_t d[BUFFER_MAX_SIZE];
    uint8_t tail;
} DataBuffer_t;

#ifdef UNIT_TEST
    extern DataBuffer_t buf;
#endif

static void ResetBuffer(DataBuffer_t *const buf);
static bool IsFull(DataBuffer_t const *const buf);
static void WriteUint8ToBuffer(DataBuffer_t *const buf, uint8_t data);
static void WriteUint16ToBuffer(DataBuffer_t *const buf, uint16_t data);
static void WriteUint32ToBuffer(DataBuffer_t *const buf, uint32_t data);
static uint8_t CheckSum(DataBuffer_t const *const buf);
STATIC void PrintBuffer(DataBuffer_t const *const buf);
STATIC void WriteDataToBuffer(LifeTester_t const *const LTChannelA,
                              LifeTester_t const *const LTChannelB);