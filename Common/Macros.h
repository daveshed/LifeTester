#ifndef MACROS_H
#define MACROS_H

#ifdef UNIT_TEST
    #define STATIC
    #define DBG_PRINT(X) 
    #define DBG_PRINTLN(X)
#else
    #define STATIC          static
    #define DBG_PRINT(X)    Serial.print(X)  
    #define DBG_PRINTLN(X)  Serial.println(X)  
#endif // UNIT_TEST

#endif // MACROS_H include guard