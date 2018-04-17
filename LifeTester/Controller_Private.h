#include "Macros.h"

#define BUFFER_MAX_SIZE   (32U)

// register map
// Bit:  7 6 5 4 3  2 1 0
// Func:           |command 
// comms register mask and bit shifts
#define EMPTY_BITS_MASK   (7U)
#define EMPTY_BITS_SHIFT  (3U)
#define COMMAND_MASK      (7U)
#define COMMAND_OFFSET    (0U)
#define RDY_BIT           (5U)
#define RW_BIT            (6U)
#define CH_SELECT_BIT     (7U)

// channel definition
#define LIFETESTER_CH_A   (0U)
#define LIFETESTER_CH_B   (1U)

// reduce resolution of params for transmit/receive
#define TIMING_BIT_SHIFT  (6U)
#define CURRENT_BIT_SHIFT (8U)
 
typedef struct DataBuffer_s {
    uint8_t d[BUFFER_MAX_SIZE];
    uint8_t tail;
    uint8_t head;
} DataBuffer_t;

typedef enum ControllerCommand_e {
    CmdReg,
    ParamsReg,
    DataReg,
    Reset,
    MaxCommands
} ControllerCommand_t;

#ifdef UNIT_TEST
    extern DataBuffer_t transmitBuffer;
    extern DataBuffer_t receiveBuffer;
    extern uint8_t      cmdReg;
    extern uint8_t settleTime;
    extern uint8_t trackDelay;
    extern uint8_t sampleTime;
    extern uint8_t thresholdCurrent;
#endif

STATIC void ResetBuffer(DataBuffer_t *const buf);
static bool IsFull(DataBuffer_t const *const buf);
STATIC void WriteUint8(DataBuffer_t *const buf, uint8_t data);
static void WriteUint16(DataBuffer_t *const buf, uint16_t data);
static void WriteUint32(DataBuffer_t *const buf, uint32_t data);
STATIC uint8_t CheckSum(DataBuffer_t const *const buf);
STATIC void PrintBuffer(DataBuffer_t const *const buf);
STATIC void WriteDataToTransmitBuffer(LifeTester_t const *const lifeTester);