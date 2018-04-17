#include "Arduino.h"
#include "Config.h"
#include "Controller.h"
#include "Controller_Private.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "StateMachine.h"
#include "Wire.h"


typedef bool LtChannel_t;

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
    memset(buf->d, 0U, BUFFER_MAX_SIZE);
    buf->tail = 0U;
    buf->head = 0U;
}

static bool IsFull(DataBuffer_t const *const buf)
{
    return !(buf->tail < BUFFER_MAX_SIZE);
}

static uint8_t NumBytes(DataBuffer_t const *const buf)
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

static void ParseNewCmdFromMaster(uint8_t newCmd)
{
    if (IsWriteModeSet(&newCmd)) // Write in the new command to action
    {
        bitDelete(newCmd, EMPTY_BITS_MASK, EMPTY_BITS_SHIFT);
        ClearReadyStatus(&newCmd);
        // go bit will be set by master here.
        cmdReg = newCmd;
    }
    else  // data stays the same just clear write bit
    {
        SetReadMode(&cmdReg);
    }
}

STATIC void WriteDataToTransmitBuffer(LifeTester_t const *const lifeTester)
{
    // TODO: handle dodgy pointers in vActive, iActive
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

/*
 Handles a read request from the master device/slave write
*/
void Controller_RequestHandler(void)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    if (IsWriteModeSet(&cmdReg))
    {
        switch(GetCommand(&cmdReg))
        {
            case GetParams:
            case GetData:
                if (IsReady(&cmdReg))
                {
                    TransmitData();
                    SetReadMode(&cmdReg);
                }
                else
                {
                    Wire.write(cmdReg);
                }
                break;
            default:
                // defensive: master shouldn't have read here
                SetReadMode(&cmdReg);
                Wire.write(cmdReg);
                break;
        }
    }
    else
    {
        // Read cmd reg requested. Allows master to poll device
        Wire.write(cmdReg);
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

/*
 Handles data write from master device/slave read
*/
void Controller_ReceiveHandler(int numBytes)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    if (!IsReady(&cmdReg))
    {
        Wire.read(); // busy - dump the command 
    }
    else
    {
        if ((GetCommand(&cmdReg) == SetParams)
            && IsWriteModeSet(&cmdReg))
        {
            ReadNewParamsFromMaster();
            SetReadMode(&cmdReg);
            SetReadyStatus(&cmdReg);
        }
        else
        {
            const uint8_t newCmd = Wire.read() ;
            ParseNewCmdFromMaster(newCmd);
        }
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

void Controller_ConsumeCommand(LifeTester_t *const lifeTesterChA,
                               LifeTester_t *const lifeTesterChB)
{
    if (IsWriteModeSet(&cmdReg))
    {
        LifeTester_t *const ltRequested = 
            (GetChannel(&cmdReg) == LIFETESTER_CH_A) ? lifeTesterChA : lifeTesterChB;
        switch (GetCommand(&cmdReg))
        {
            case Reset:
                StateMachine_Reset(ltRequested);
                break;
            case GetParams:
                WriteParamsToTransmitBuffer();
                break;
            case GetData:
                WriteDataToTransmitBuffer(ltRequested);
                break;
            case SetParams:
            default:
                break;
        }
        SetReadMode(&cmdReg);
        SetReadyStatus(&cmdReg);
    }
    else
    {
        // no command to consume - request handler will return cmdReg in read
    }
}