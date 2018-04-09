#ifndef MACROS_H
#define MACROS_H

#ifdef UNIT_TEST
    #include <stdio.h>
    #define STATIC
    #define SERIAL_PRINT(MSG, FMT)    printf(FMT, MSG)
    #define SERIAL_PRINTLN(MSG, FMT)  printf(FMT "\n", MSG)
    #define SERIAL_PRINTLNEND()       printf("\n")
#else
    #define STATIC                    static
    #define SERIAL_PRINT(MSG, FMT)    Serial.print(MSG)  
    #define SERIAL_PRINTLN(MSG, FMT)  Serial.println(MSG)  
    #define SERIAL_PRINTLNEND()       Serial.println()
#endif // UNIT_TEST

#ifdef DEBUG
    #define DBG_PRINT(MSG, FMT)       SERIAL_PRINT(MSG, FMT)
    #define DBG_PRINTLN(MSG, FMT)     SERIAL_PRINTLN(MSG, FMT)
    #define DBG_PRINTLNEND()          SERIAL_PRINTLNEND()
#else
    #define DBG_PRINT(MSG, FMT)
    #define DBG_PRINTLN(MSG, FMT)
    #define DBG_PRINTLNEND()
#endif // DEBUG

#endif // MACROS_H include guard