#ifndef I2C_H
#define I2C_H


#define MAX_DATA_SIZE     8 //8 bytes for a uint64_t
#define BUFFER_ENTRIES    9
#define BUFFER_MAX_SIZE   MAX_DATA_SIZE * BUFFER_ENTRIES

//Declaration of array to hold data to send over I2C
extern uint8_t I2CByteBuffer[BUFFER_MAX_SIZE];

void I2C_ClearArray(uint8_t byteArray[], const uint8_t numBytes);
void I2C_PackIntToBytes(const uint64_t data, uint8_t byteArray[], const uint8_t numBytes);
void I2C_PrintByteArray(const uint8_t byteArray[], const uint16_t n);
void I2C_TransmitData(void);
void I2C_PrepareData(void);

#endif
