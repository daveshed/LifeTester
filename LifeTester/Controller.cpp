#include "Arduino.h"
#include "Config.h"
#include "Controller.h"
#include "Controller_Private.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "Wire.h"

typedef enum ControllerCommand_e {
    None,
    Reset,
    SetParams,
    GetParams,
    GetData,
    MaxCommands
} ControllerCommand_t;

ControllerCommand_t cmdRequest;
STATIC DataBuffer_t transmitBuffer;
STATIC DataBuffer_t receiveBuffer;
static uint8_t CommandReg;

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

void Controller_WriteDataToBuffer(LifeTester_t const *const LTChannelA,
                                  LifeTester_t const *const LTChannelB)
{
    ResetBuffer(&transmitBuffer);
    WriteUint32(&transmitBuffer, LTChannelA->timer);
    WriteUint8(&transmitBuffer, *LTChannelA->data.vActive);
    WriteUint16(&transmitBuffer, *LTChannelA->data.iActive);
    WriteUint8(&transmitBuffer, *LTChannelB->data.vActive);
    WriteUint16(&transmitBuffer, *LTChannelB->data.iActive);
    WriteUint16(&transmitBuffer, TempGetRawData());
    WriteUint16(&transmitBuffer, analogRead(LIGHT_SENSOR_PIN));
    WriteUint8(&transmitBuffer, (uint8_t)LTChannelA->error);
    WriteUint8(&transmitBuffer, (uint8_t)LTChannelB->error);
    WriteUint8(&transmitBuffer, CheckSum(&transmitBuffer));
}

void Controller_TransmitData(void)
{
    Wire.write(transmitBuffer.d, NumBytes(&transmitBuffer));
}

void Controller_RequestHandler(void)
{
    switch(cmdRequest)
    {
        case (None):
            // Ready for a write to the command register
            CommandReg = Wire.read();
            break;
        case (SetParams):
            // Ready to receive data into settings register
            ResetBuffer(&receiveBuffer);
            while (Wire.available())
            {
                WriteUint8(&receiveBuffer, Wire.read());
                NumBytes(&receiveBuffer);
            }
            break;
        case (GetParams):
            // Ready to transmit measurement params
            Wire.write(transmitBuffer.d, NumBytes(&transmitBuffer));
            break;
        case (GetData):
            // Request to transmit data
            Controller_TransmitData();
            break;
        case (Reset):
            /*Reset event - resets all channels. Note that the mode is
            set here and can be read with a getter from main*/
        default:
            break;
    }
}
