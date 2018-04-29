#include "Arduino.h"
#include "Config.h"
#include "Controller.h"
#include "Controller_Private.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "StateMachine.h"
#include "Wire.h"

STATIC DataBuffer_t transmitBuffer;
STATIC uint8_t      cmdReg;
static bool         cmdRegReadRequested = false;
    
STATIC void ResetBuffer(DataBuffer_t *const buf)
{
    memset(buf->d, EMPTY_BYTE, BUFFER_MAX_SIZE);
    buf->tail = 0U;
    buf->head = 0U;
}

STATIC uint8_t NumBytes(DataBuffer_t const *const buf)
{
    return buf->tail - buf->head;
}

static bool IsFull(DataBuffer_t const *const buf)
{
    return !(buf->tail < BUFFER_MAX_SIZE);
}

STATIC bool IsEmpty(DataBuffer_t const *const buf)
{
    return (NumBytes(buf) == 0U);
}

static uint16_t ReadUint16(void)
{
    const uint8_t lsb = Wire.read(); 
    const uint8_t msb = Wire.read();
    return ((lsb & 0xFF) | ((msb & 0xFF) << 8U));
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

static void TransmitData(void)
{
    Wire.write(transmitBuffer.d, NumBytes(&transmitBuffer));
}

static void FlushReadBuffer(void)
{
    while (Wire.available())
    {
        Wire.read();
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
    WriteUint16(&transmitBuffer, Config_GetSettleTime());
    WriteUint16(&transmitBuffer, Config_GetTrackDelay());
    WriteUint16(&transmitBuffer, Config_GetSampleTime());
    WriteUint16(&transmitBuffer, Config_GetThresholdCurrent());
}

/*
 Sets measurement parameters from byte width variables.
*/
static void ReadNewParamsFromMaster(void)
{
    Config_SetSettleTime(ReadUint16());
    Config_SetTrackDelay(ReadUint16());
    Config_SetSampleTime(ReadUint16());
    Config_SetThresholdCurrent(ReadUint16());
}
/*
 Copies everything except rdy bit and clears any error codes 
*/
static void LoadNewCmdToReg(uint8_t newCmdReg)
{
    bitCopy(cmdReg, newCmdReg, CH_SELECT_BIT);
    bitCopy(cmdReg, newCmdReg, RW_BIT);
    bitDelete(cmdReg, ERROR_MASK, ERROR_OFFSET);
    SET_COMMAND(cmdReg, GET_COMMAND(newCmdReg));
}

static void UpdateStatusBits(uint8_t newCmdReg)
{
    const ControllerCommand_t c = GET_COMMAND(newCmdReg);
    // set status bits
    if (IS_WRITE(newCmdReg))
    {
        switch (c)
        {
            case Reset:
            case ParamsReg:
            case CmdReg:
                CLEAR_RDY_STATUS(cmdReg);  // only applies for reading/loading
                break;
            case DataReg: // Master can't write to the data register
            default:
                SET_ERROR(cmdReg, UnkownCmdError);
                break;
        }
    }
    else  // read requested
    {
        switch (c)
        {
            case Reset:
                CLEAR_RDY_STATUS(cmdReg);
                break;
            case ParamsReg:
            case DataReg:
                // data requested - need to load into buffer now. Set busy
                CLEAR_RDY_STATUS(cmdReg);
                break;
            case CmdReg:  // command not loaded - preserve reg as is for reading
                break;
            default:
                SET_ERROR(cmdReg, UnkownCmdError);
                break;
        }
    }
}

void Controller_Init(void)
{
    ResetBuffer(&transmitBuffer);
    cmdReg = 0U;
    SET_RDY_STATUS(cmdReg);
    FlushReadBuffer();
    cmdRegReadRequested = false;
}

/*
 Handles a read request from the master device/slave write
*/
void Controller_RequestHandler(void)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    if (cmdRegReadRequested)
    {
        cmdRegReadRequested = false;
        Wire.write(cmdReg);
    }
    else
    {
        if (!IsEmpty(&transmitBuffer))
        {
            TransmitData();
        }
        else
        {
            SET_ERROR(cmdReg, BusyError);
        }
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

/*
 Handles data write from master device/slave read
*/
void Controller_ReceiveHandler(int numBytes)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    /*
     writing new measurement parameters - not a command.
     Note that all params MUST be written in a single transaction and polling
     can't be done here (not necessary). How would we know the difference bet-
     ween a request to read the cmd reg and the actual params being written.*/
    if ((GET_COMMAND(cmdReg) == ParamsReg)
        && IS_WRITE(cmdReg))
    {
        if (numBytes == PARAMS_REG_SIZE)
        {
            ReadNewParamsFromMaster();
            // protect from another write without command
            SET_READ_MODE(cmdReg);
        }
        else
        {
            // chuck away bad settings - wrong size
            FlushReadBuffer();
            SET_ERROR(cmdReg, BadParamsError);
        }
    }
    else // new command isued...
    {
        const uint8_t newCmdReg = Wire.read();
        // Make sure old commands don't fill up buffer
        FlushReadBuffer();
        // requesting write to cmd reg
        if (GET_COMMAND(newCmdReg) == CmdReg)
        {
            if (IS_WRITE(newCmdReg))
            {
                if (IS_RDY(cmdReg))
                {
                    LoadNewCmdToReg(newCmdReg);
                }
                else
                {
                    SET_ERROR(cmdReg, BusyError);
                }
            }
            // Master requested read command reg - see request handler
            else
            {
                cmdRegReadRequested = true;
            }
        }
        // write cmd already requested now receiving new command 
        else if (GET_COMMAND(cmdReg) == CmdReg)
        {
            LoadNewCmdToReg(newCmdReg);
            UpdateStatusBits(newCmdReg);
        }
        else
        {
            // TODO: handle this. received undefined command
        }
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

void Controller_ConsumeCommand(LifeTester_t *const lifeTesterChA,
                               LifeTester_t *const lifeTesterChB)
{
    LifeTester_t *const ch = 
        (GET_CHANNEL(cmdReg) == LIFETESTER_CH_A) ? lifeTesterChA : lifeTesterChB;
    switch (GET_COMMAND(cmdReg))
    {
        case Reset:
            if (!IS_RDY(cmdReg))  // RW bit ignored
            {
                StateMachine_Reset(ch);
                SET_RDY_STATUS(cmdReg);
            }
            break;
        case ParamsReg:
            if (!IS_WRITE(cmdReg))
            {
                WriteParamsToTransmitBuffer();
                SET_RDY_STATUS(cmdReg);
            }
            else
            {
                FlushReadBuffer();
                SET_RDY_STATUS(cmdReg);
            }
            break;
        case DataReg:
            if (!IS_WRITE(cmdReg))
            {
                if (!IS_RDY(cmdReg))
                {
                    // ensure data isn't loaded again
                    WriteDataToTransmitBuffer(ch);
                    SET_RDY_STATUS(cmdReg);                    
                }
            }
            break;
        default:
            break;
    }
}