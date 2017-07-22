#include <SPI.h>
#include <Wire.h>
#include "LEDFlash.h"
#include "MCP4822.h"
#include "ADS1286.h"
#include "MAX6675.h"

//1 for serial communication, 0 to turn off.
#define DEBUG 0

////////////////
//pin settings//
////////////////
const int ADC_A_CS_pin  = 10;  //ADC_A chip select. B is on pin8 
const int ADC_B_CS_pin  = 8;  //ADC_A chip select. B is on pin8 
const int DAC_CS_pin    = 9; //DAC chip select 
const int Led_A_pin     = 6;  //LED indicator
const int Led_B_pin     = 5; //LED indicator
const int MAX_CS_pin    = 7; //chip select for the MAX6675
const int LDR_pin       = 0; //light intensity on A0

typedef enum {ok, low_current, current_limit, threshold, DAC_error} error_code;

typedef struct IV {
  unsigned long v;       //v_cur and v_next??
  unsigned long p_cur;
  unsigned long p_next;
  unsigned long i_cur;
  unsigned long i_next;
  unsigned long i_transmit;
} IV;

typedef struct lifetester {
  char          DAC_channel;
  Flasher       Led;
  ADS1286       ADC_input;
  int           n_reads_cur,
                n_reads_next,   //counting number of readings taken by ADC during sampling window
                n_error_reads;  //number of readings outside allowed limits
  error_code    error;          
  IV            iv_data;        //holds operating points
  unsigned long timer;          //timer for tracking loop
} lifetester;

////////////////
//DAC settings//
////////////////
byte DAC_errmsg; //error code from DACset
const char DACgain = 'l'; //gain setting for DAC

///////////
//Globals//
///////////
const int I2C_address = 8; //address for this slave device
const int tolerance = 20; //number of allowed bad readings before there is an error
const int V_scan_min = 0;
const int V_scan_max = 2700;
const int dV_scan = 40; //step size in MPP scan
const int dV_MPPT = 5; //step size during MPPT - note that if this is too small, there won't be a noticeable change in power between points and the alogorithm won't work.
const unsigned int i_threshold = 50; //required threshold ADCreading in MPPscan for test to start
byte I2C_data[14] = {0}; //array to hold data to send over I2C

//////////////////////
//measurement timing//
//////////////////////
const int settle_time = 200; //settle time after setting DAC to ADC measurement
const int sampling_time = 200; //time interval over which ADC measurements are made continuously then averaged afterward
const int tracking_delay = 200; //time period between tracking measurements

///////////////////////
//function prototypes//
///////////////////////
void MPP_scan(lifetester *channel_x, int startV, int finV, int dV, MCP4822 &DAC);
void MPP_update(lifetester *channel_x, MCP4822 &DAC);
void request_event(void); //I2C sender
void I2C_transmit(unsigned int data); //helper function

///////////////////
//class instances//
///////////////////
MCP4822 DAC_SMU(DAC_CS_pin); 
MAX6675 T_sense(MAX_CS_pin);

//////////////////////////////////
//Initialise lifetester channels//
////////////////////////////////// 
lifetester  channel_A = {
  'a',
  Flasher(Led_A_pin),
  ADS1286(ADC_A_CS_pin),
  0,
  0,
  0,
  ok,
  {0},
  0
};
lifetester  channel_B = {
  'b',
  Flasher(Led_B_pin),
  ADS1286(ADC_B_CS_pin),
  0,
  0,
  0,
  ok,
  {0},
  0
};

