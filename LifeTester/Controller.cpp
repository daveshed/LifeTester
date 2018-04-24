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

static void LoadNewCmdToReg(uint8_t newCmdReg)
{
    bitCopy(cmdReg, newCmdReg, CH_SELECT_BIT);
    bitCopy(cmdReg, newCmdReg, RW_BIT);
    bitCopy(cmdReg, newCmdReg, GO_BIT);
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
                IS_GO(cmdReg) ? CLEAR_RDY_STATUS(cmdReg)
                              : SET_RDY_STATUS(cmdReg);
                break;
            default:
                CLEAR_RDY_STATUS(cmdReg);  // only applies for reading/loading
                SET_RDY_STATUS(cmdReg); 
                break;
        }
    }
    else  // read requested
    {
        switch (c)
        {
            case Reset:
                IS_GO(cmdReg) ? CLEAR_RDY_STATUS(cmdReg)
                              : SET_RDY_STATUS(cmdReg);
                break;
            case ParamsReg:
            case DataReg:
                // data requested - need to load into buffer now. Set busy
                if (IS_GO(cmdReg))
                {
                    CLEAR_RDY_STATUS(cmdReg);
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
        SET_READ_MODE(cmdReg);
    }
    else
    {
        const uint8_t newCmdReg = Wire.read();
        // requesting write to cmd reg
        if (GET_COMMAND(newCmdReg) == CmdReg)
        {
            if (IS_WRITE(newCmdReg))
            {
                if (IS_RDY(cmdReg))
                {
                    LoadNewCmdToReg(newCmdReg);
                }
            }
            else  //read
            {
                ResetBuffer(&transmitBuffer);
                WriteUint8(&transmitBuffer, cmdReg);
            }
        }
        // receiving new command 
        else if (GET_COMMAND(cmdReg) == CmdReg)
        {
            LoadNewCmdToReg(newCmdReg);
            UpdateStatusBits(newCmdReg);
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
    LifeTester_t *const ch = 
        (GET_CHANNEL(cmdReg) == LIFETESTER_CH_A) ? lifeTesterChA : lifeTesterChB;
    switch (GET_COMMAND(cmdReg))
    {
        case Reset:
            if (IS_GO(cmdReg))
            {
                StateMachine_Reset(ch);
                CLEAR_GO_STATUS(cmdReg);
            }
            SET_RDY_STATUS(cmdReg);
            break;
        case ParamsReg:
            if (IS_GO(cmdReg) && !IS_WRITE(cmdReg))
            {
                WriteParamsToTransmitBuffer();
                // Ready for read.
                SET_RDY_STATUS(cmdReg);
                // ensure data isn't loaded again
                CLEAR_GO_STATUS(cmdReg);
            }
            break;
        case DataReg:
            if (IS_GO(cmdReg) && !IS_WRITE(cmdReg))
            {
                WriteDataToTransmitBuffer(ch);
                // Ready for read.
                SET_RDY_STATUS(cmdReg);                    
                // ensure data isn't loaded again
                CLEAR_GO_STATUS(cmdReg);
            }
            break;
        default:
            break;
    }
}