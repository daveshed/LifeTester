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
STATIC DataBuffer_t receiveBuffer;
STATIC uint8_t      commsReg;

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

static ControllerCommand_t GetCommand()
{
    return (ControllerCommand_t)bitExtract(commsReg, COMMAND_MASK, COMMAND_OFFSET);
}

static LtChannel_t GetChannel()
{
    return bitRead(commsReg, CH_SELECT_BIT);
}

static void SetDataReady(void)
{
    bitSet(commsReg, DATA_RDY_BIT);
}

static bool DataReady(void)
{
    return bitRead(commsReg, DATA_RDY_BIT);
}

static void ClearDataReady(void)
{
    bitClear(commsReg, DATA_RDY_BIT);
}

static void SetCmdDone()
{
    bitSet(commsReg, CMD_DONE_BIT);
}

static bool CmdDone()
{
    return bitRead(commsReg, CMD_DONE_BIT);
}

static void ClearCmdDone()
{
    bitClear(commsReg, CMD_DONE_BIT);
}

STATIC void WriteDataToTransmitBuffer(LifeTester_t const *const lifeTester)
{
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
    #if 0
    WriteUint8(&transmitBuffer, Config_GetSettleTime() >> TIMING_BIT_SHIFT);
    WriteUint8(&transmitBuffer, Config_GetTrackDelay() >> TIMING_BIT_SHIFT);
    WriteUint8(&transmitBuffer, Config_GetSampleTime() >> TIMING_BIT_SHIFT);
    WriteUint8(&transmitBuffer, Config_GetThresholdCurrent() >> CURRENT_BIT_SHIFT);
    #endif
}

static void ReadParamsFromReceiveBuffer(void)
{
    // Not implemented yet
}

static void TransmitData(void)
{
    Wire.write(transmitBuffer.d, NumBytes(&transmitBuffer));
}

static void ReceiveData(void)
{
    while (Wire.available())
    {
        WriteUint8(&receiveBuffer, Wire.read());
        NumBytes(&receiveBuffer);
    }
}
/*
 Handles a request for data from the master device
*/
void Controller_RequestHandler(void)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    switch(GetCommand())
    {
        case GetParams:
        case GetData:
            // Ready to transmit params/data - already in buffer
            if (DataReady())
            {
                TransmitData();
                SetCmdDone();
            }
            break;
        case GetCommsReg:
        default:
            Wire.write(commsReg);
            break;
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

/*
 Handles data received from master device
*/
void Controller_ReceiveHandler(int numBytes)
{
    digitalWrite(COMMS_LED_PIN, HIGH);
    if (CmdDone())
    {
        // last command done so ready to receive another one.
        commsReg = bitExtract(Wire.read(), COMMAND_MASK, COMMAND_OFFSET);
        ResetBuffer(&transmitBuffer);
        ResetBuffer(&receiveBuffer);
    }
    else
    {
        switch(GetCommand())
        {
            case SetParams:
                // Ready to receive data into settings register
                ReceiveData();
                SetDataReady();
                break;
            default:
                // overwrites comms reg if master writes after other commands
                commsReg = bitExtract(Wire.read(), COMMAND_MASK, COMMAND_OFFSET);
                break;
        }
    }
    digitalWrite(COMMS_LED_PIN, LOW);
}

void Controller_ConsumeCommand(LifeTester_t *const lifeTesterChA,
                               LifeTester_t *const lifeTesterChB)
{
    if (!CmdDone())
    {
        LifeTester_t *const ltRequested = 
            (GetChannel() == LIFETESTER_CH_A) ? lifeTesterChA : lifeTesterChB;
        switch (GetCommand())  // get last command stored in reg
        {
            case Reset:
                StateMachine_Reset(ltRequested);
                SetCmdDone();
                break;
            case SetParams:
                if (DataReady())
                {
                    ReadParamsFromReceiveBuffer();
                    SetCmdDone();
                }
                break;
            case GetParams:
                // WriteParamsToTransmitBuffer();
                SetDataReady();
                break;
            case GetData:
                // WriteDataToTransmitBuffer(ltRequested);
                SetDataReady();
                break;
            default:
                SetCmdDone();
                break;
        }
    }
    else
    {
        // bitDelete(commsReg, COMMAND_MASK, COMMAND_OFFSET);
    }
}
