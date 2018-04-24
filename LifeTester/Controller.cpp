#include "Arduino.h"
#include "Config.h"
#include "Controller.h"
#include "Controller_Private.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "StateMachine.h"
#include "Wire.h"


ControllerCommand_t cmdRequest;
STATIC DataBuffer_t transmitBuffer;
STATIC uint8_t      cmdReg;
static uint8_t      newCmdReg;

// TODO: replace with getters/setters in config.c
STATIC uint8_t settleTime;
STATIC uint8_t trackDelay;
STATIC uint8_t sampleTime;
STATIC uint8_t thresholdCurrent;


STATIC void ResetBuffer(DataBuffer_t *const buf)
{
    memset(buf->d, EMPTY_BYTE, BUFFER_MAX_SIZE);
    buf->tail = 0U;
    buf->head = 0U;
}

static bool IsFull(DataBuffer_t const *const buf)
{
    return !(buf->tail < BUFFER_MAX_SIZE);
}

STATIC uint8_t NumBytes(DataBuffer_t const *const buf)
{
    return buf->tail - buf->head;
}

STATIC void WriteUint8(DataBuffer_t *const buf, uint8_t data)
{
    if (!IsFull(buf))
    {
        buf->d[buf->tail] = data;
        buf->tail++;
    }
    // what do if it is full?? - assert?
}

static void WriteUint16(DataBuffer_t *const buf, uint16_t data)
{
    WriteUint8(buf, (data & 0xFFU));
    WriteUint8(buf, ((data >> 8U) & 0xFFU));
}

static void WriteUint32(DataBuffer_t *const buf, uint32_t data)
{
    for (int i = 0; i < sizeof(uint32_t); i++)
    {
        const uint8_t byteToWrite = (data >> (8U * i)) & 0xFFU;
        WriteUint8(buf, byteToWrite);
    }
}

STATIC uint8_t CheckSum(DataBuffer_t const *const buf)
{
    uint8_t checkSum = 0U;
    int i = buf->head;
    while (i <= buf->tail)
    {
        checkSum += buf->d[i];
        i++;
    }
    return checkSum;
}

STATIC void PrintBuffer(DataBuffer_t const *const buf)
{
    SERIAL_PRINT("buffer =", "%s");
    for (int i = 0; i < BUFFER_MAX_SIZE; i++)
    {
        SERIAL_PRINT(" ", "%s");
        SERIAL_PRINT(buf->d[i], "%u");
    }
    SERIAL_PRINTLNEND();
}

static ControllerCommand_t GetCommand(const uint8_t *reg)
{
    return (ControllerCommand_t)bitExtract(*reg, COMMAND_MASK, COMMAND_OFFSET);
}

static void SetCommand(uint8_t *reg, ControllerCommand_t cmd)
{
    bitInsert(*reg, (uint8_t)cmd, COMMAND_MASK, COMMAND_OFFSET);
}

static LtChannel_t GetChannel(const uint8_t *reg)
{
    return bitRead(*reg, CH_SELECT_BIT);
}

static void SetReadyStatus(uint8_t *reg)
{
    bitSet(*reg, RDY_BIT);
}

static bool IsReady(const uint8_t *reg)
{
    return bitRead(*reg, RDY_BIT);
}

static void ClearReadyStatus(uint8_t *reg)
{
    bitClear(*reg, RDY_BIT);
}

static void SetGoStatus(uint8_t *reg)
{
    bitSet(*reg, GO_BIT);
}

static bool IsGo(const uint8_t *reg)
{
    return bitRead(*reg, GO_BIT);
}

static void ClearGoStatus(uint8_t *reg)
{
    bitClear(*reg, GO_BIT);
}

static bool IsWriteModeSet(const uint8_t *reg)
{
    return bitRead(*reg, RW_BIT);
}

static void SetReadMode(uint8_t *reg)
{
    bitClear(*reg, RW_BIT);
}

static void SetWriteMode(uint8_t *reg)
{
    bitClear(*reg, RW_BIT);
}

STATIC void WriteDataToTransmitBuffer(LifeTester_t const *const lifeTester)
{
    // TODO: handle dodgy pointers in vActive, iActive
    ResetBuffer(&transmitBuffer);
    WriteUint32(&transmitBuffer, lifeTester->timer);
    WriteUint8(&transmitBuffer, *lifeTester->data.vActive);
    WriteUint16(&transmitBuffer, *lifeTester->data.iActive);
    WriteUint16(&transmitBuffer, TempGetRawData());
    WriteUint16(&transmitBuffer, analogRead(LIGHT_SENSOR_PIN));
    WriteUint8(&transmitBuffer, (uint8_t)lifeTester->error);
    WriteUint8(&transmitBuffer, CheckSum(&transmitBuffer));
}

static void WriteParamsToTransmitBuffer(void)
{
    ResetBuffer(&transmitBuffer);
    WriteUint8(&transmitBuffer, settleTime);
    WriteUint8(&transmitBuffer, trackDelay);
    WriteUint8(&transmitBuffer, sampleTime);
    WriteUint8(&transmitBuffer, thresholdCurrent);
}

static void ReadNewParamsFromMaster(void)
{
    settleTime = Wire.read();
    trackDelay = Wire.read();
    sampleTime = Wire.read();
    thresholdCurrent = Wire.read();
}

static void TransmitData(void)
{
    Wire.write(transmitBuffer.d, NumBytes(&transmitBuffer));
}

static void LoadNewCmdToReg(void)
{
    bitCopy(cmdReg, newCmdReg, CH_SELECT_BIT);
    bitCopy(cmdReg, newCmdReg, RW_BIT);
    bitCopy(cmdReg, newCmdReg, GO_BIT);
    SET_COMMAND(cmdReg, GET_COMMAND(newCmdReg));
}

static void ParseNewCmdRegFromMaster(void)
{
    const ControllerCommand_t c = GET_COMMAND(newCmdReg);
    // set status bits
    if (IsWriteModeSet(&newCmdReg))
    {
        // LoadNewCmdToReg();
        switch (c)
        {
            case Reset:
                IsGo(&cmdReg) ? ClearReadyStatus(&cmdReg)
                              : SetReadyStatus(&cmdReg);
                break;
            default:
                ClearGoStatus(&cmdReg);  // only applies for reading/loading
                SetReadyStatus(&cmdReg); 
                break;
        }
    }
    else  // read requested
    {
        switch (c)
        {
            case Reset:
                // LoadNewCmdToReg();
                IsGo(&cmdReg) ? ClearReadyStatus(&cmdReg)
                              : SetReadyStatus(&cmdReg);
                break;
            case ParamsReg:
            case DataReg:
                // LoadNewCmdToReg();
                // data requested - need to load into buffer now. Set busy
                if (IsGo(&cmdReg))
                {
                    ClearReadyStatus(&cmdReg);
                }
                break;
            case CmdReg:  // command not loaded - preserve reg as is for reading
            default:
                break;
        }
    }

    // TODO: handle bad command
}

/*
 Handles a read request from the master device/slave write
*/
void Controller_RequestHandler(void)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    TransmitData();
    digitalWrite(COMMS_LED_PIN, LOW);
}

/*
 Handles data write from master device/slave read
*/
// TODO: use numbytes arg - consume all data? Check not reading when no data is there.
void Controller_ReceiveHandler(int numBytes)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    // writing new measurement parameters - not a command
    if ((GET_COMMAND(cmdReg) == ParamsReg) && IS_WRITE(cmdReg))
    {
        ReadNewParamsFromMaster();
        // protect from another write without command
        SetReadMode(&cmdReg);
    }
    else
    {
        newCmdReg = Wire.read();
        // requesting write to cmd reg
        if (GET_COMMAND(newCmdReg) == CmdReg)
        {
            if (IS_WRITE(newCmdReg))
            {
                if (IS_RDY(cmdReg))
                {
                    LoadNewCmdToReg();
                }
            }
            else  //read
            {
                WriteUint8(&transmitBuffer, cmdReg);
            }
        }
        // receiving new command 
        else if (GET_COMMAND(cmdReg) == CmdReg)
        {
            LoadNewCmdToReg();
            ParseNewCmdRegFromMaster();  // TODO - rename     
        }
        else
        {
            // received undefined command
        }
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

void Controller_ConsumeCommand(LifeTester_t *const lifeTesterChA,
                               LifeTester_t *const lifeTesterChB)
{
    LifeTester_t *const ltRequested = 
        (GetChannel(&cmdReg) == LIFETESTER_CH_A) ? lifeTesterChA : lifeTesterChB;
    switch (GetCommand(&cmdReg))
    {
        case Reset:
            if (IsGo(&cmdReg))
            {
                StateMachine_Reset(ltRequested);
                ClearGoStatus(&cmdReg);
            }
            SetReadyStatus(&cmdReg);
            break;
        case ParamsReg:
            if (IsGo(&cmdReg) && !IsWriteModeSet(&cmdReg))
            {
                WriteParamsToTransmitBuffer();
                // Ready for read.
                SetReadyStatus(&cmdReg);
                // ensure data isn't loaded again
                ClearGoStatus(&cmdReg);
            }
            break;
        case DataReg:
            if (IsGo(&cmdReg) && !IsWriteModeSet(&cmdReg))
            {
                WriteDataToTransmitBuffer(ltRequested);
                // Ready for read.
                SetReadyStatus(&cmdReg);                    
                // ensure data isn't loaded again
                ClearGoStatus(&cmdReg);
            }
            break;
        default:
            break;
    }
}