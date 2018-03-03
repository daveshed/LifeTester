/*
 Data accessible from tests that mocks arduino digital io and timer outputs.
*/

#ifndef MOCKARDUINO_H
#define MOCKARDUINO_H

#define N_DIGITAL_PINS          (14U)

// types to hold information about mock digital pins.
typedef enum pinAssignment_e{
    input,
    output,
    unassigned
} pinAssignment_t;

typedef struct pinState_s{
    pinAssignment_t mode;
    bool            outputOn;
} pinState_t;

/*
 Declaring extern variables. Any file that includes this header will be able to
 access them. These data mock io and timers of the Arduino microcontroller for 
 testing only. 
 */
extern long unsigned int mockMillis;
extern pinState_t mockDigitalPins[N_DIGITAL_PINS];

#endif // MOCKARDUINO_H