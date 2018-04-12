#include "Arduino.h"
#include "Config.h"
#include "Controller.h"
#include "Controller_Private.h"
#include "IoWrapper.h"
#include "LifeTesterTypes.h"
#include "Wire.h"

/*
function that executes whenever data is requested by master
this function is registered as an event, see setup()
all other code is interrupted and the data sent to master 
send data as a sequence of 17 bytes...

Variable   n Bytes
timer      4
v_a        1
i_a        2
v_b        1
i_b        2
T          2
I_light    2
error_A    1
error B    1
Checksum   1
TOTAL -> 17 Bytes
*/

STATIC DataBuffer_t buf;

static void ResetBuffer(DataBuffer_t *const buf)
{
    memset(buf->d, 0U, BUFFER_MAX_SIZE);
    buf->tail = 0U;
}

static bool IsFull(DataBuffer_t const *const buf)
{
    return !(buf->tail < BUFFER_MAX_SIZE);
}

static void WriteUint8ToBuffer(DataBuffer_t *const buf, uint8_t data)
{
    if (!IsFull(buf))
    {
        buf->d[buf->tail] = data;
        buf->tail++;
    }
}

static void WriteUint16ToBuffer(DataBuffer_t *const buf, uint16_t data)
{
    WriteUint8ToBuffer(buf, (data & 0xFFU));
    WriteUint8ToBuffer(buf, ((data >> 8U) & 0xFFU));
}

static void WriteUint32ToBuffer(DataBuffer_t *const buf, uint32_t data)
{
    for (int i = 0; i < sizeof(uint32_t); i++)
    {
        const uint8_t byteToWrite = (data >> (8U * i)) & 0xFFU;
        WriteUint8ToBuffer(buf, byteToWrite);
    }
}

static uint8_t CheckSum(DataBuffer_t const *const buf)
{
    uint8_t checkSum = 0U;
    for (int i = 0; i < buf->tail; i++)
    {
        checkSum += buf->d[i];
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

STATIC void WriteDataToBuffer(LifeTester_t const *const LTChannelA,
                              LifeTester_t const *const LTChannelB)
{
    ResetBuffer(&buf);
    WriteUint32ToBuffer(&buf, LTChannelA->timer);
    WriteUint8ToBuffer(&buf, *LTChannelA->data.vActive);
    WriteUint16ToBuffer(&buf, *LTChannelA->data.iActive);
    WriteUint8ToBuffer(&buf, *LTChannelB->data.vActive);
    WriteUint16ToBuffer(&buf, *LTChannelB->data.iActive);
    WriteUint16ToBuffer(&buf, TempGetRawData());
    WriteUint16ToBuffer(&buf, analogRead(LIGHT_SENSOR_PIN));
    WriteUint8ToBuffer(&buf, (uint8_t)LTChannelA->error);
    WriteUint8ToBuffer(&buf, (uint8_t)LTChannelB->error);
    WriteUint8ToBuffer(&buf, CheckSum(&buf));
}

void Controller_TransmitData(void)
{
    Wire.write(buf.d, buf.tail);  
}