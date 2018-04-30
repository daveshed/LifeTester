#ifndef MOCKARDUINO_H
#define MOCKARDUINO_H

// digital pin number takes a value between 0-13.
#define N_DIGITAL_PINS          (14U)
// analog pin number takes a value between 0-5.
#define N_ANALOG_PINS           (6U)

typedef struct DigitalPinState_s {
    bool isAssigned;
    bool isOutput;
    bool isOn;
} DigitalPinState_t;

bool IsDigitalPinAssigned(uint8_t pin);
bool IsDigitalPinOutput(uint8_t pin);
bool IsDigitalPinHigh(uint8_t pin);
bool IsDigitalPinLow(uint8_t pin);
void ResetDigitalPins(void);
void SetDigitalPinToOutput(uint8_t pin);
void SetDigitalPinToInput(uint8_t pin);
void SetDigitalPinOutputHigh(uint8_t pin);
void SetDigitalPinOutputLow(uint8_t pin);
#endif // MOCKARDUINO_H