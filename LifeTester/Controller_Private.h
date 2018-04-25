#include "Macros.h"

#define BUFFER_MAX_SIZE   (32U)
#define DATA_SEND_SIZE    (13U)  // size of data sent for single channel

// register map
// Bit:  7  6   5  4  3  2  1  0
// Func: Ch RW RDY X  X |  CMD  |
// comms register mask and bit shifts
#define COMMAND_MASK      (7U)
#define COMMAND_OFFSET    (0U)
#define UNUSED_MASK       (3U)
#define UNUSED_OFFSET     (3U)
#define RDY_BIT           (5U)
#define RW_BIT            (6U)
#define CH_SELECT_BIT     (7U)

// channel definition
#define LIFETESTER_CH_A   (0U)
#define LIFETESTER_CH_B   (1U)
 
// byte requested but no data to return
#define EMPTY_BYTE        (0xFF)

typedef bool LtChannel_t;

// Commands
#define CLEAR_GO_STATUS(REG)    bitClear(REG, GO_BIT)
#define CLEAR_RDY_STATUS(REG)   bitClear(REG, RDY_BIT)
#define GET_COMMAND(REG) \
    (ControllerCommand_t)(bitExtract(REG, COMMAND_MASK, COMMAND_OFFSET))
#define GET_CHANNEL(REG)        (LtChannel_t)bitRead(REG, CH_SELECT_BIT)
#define IS_GO(REG)              bitRead(REG, GO_BIT)
#define IS_RDY(REG)             bitRead(REG, RDY_BIT)
#define IS_WRITE(REG)           bitRead(REG, RW_BIT)
#define SET_CHANNEL(REG, CH)    bitWrite(REG, CH_SELECT_BIT, CH)
#define SET_GO_STATUS(REG)      bitSet(REG, GO_BIT)
#define SET_RDY_STATUS(REG)     bitSet(REG, RDY_BIT)
#define SET_READ_MODE(REG)      bitClear(REG, RW_BIT)
#define SET_WRITE_MODE(REG)     bitClear(REG, RW_BIT)
#define SET_COMMAND(REG, CMD) \
    bitInsert(REG, (uint8_t)CMD, COMMAND_MASK, COMMAND_OFFSET)

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
#endif

STATIC uint8_t NumBytes(DataBuffer_t const *const buf);
STATIC void ResetBuffer(DataBuffer_t *const buf);
static bool IsFull(DataBuffer_t const *const buf);
STATIC void WriteUint8(DataBuffer_t *const buf, uint8_t data);
static void WriteUint16(DataBuffer_t *const buf, uint16_t data);
static void WriteUint32(DataBuffer_t *const buf, uint32_t data);
STATIC uint8_t CheckSum(DataBuffer_t const *const buf);
STATIC void PrintBuffer(DataBuffer_t const *const buf);
STATIC void WriteDataToTransmitBuffer(LifeTester_t const *const lifeTester);