#ifndef MACROS_H
#define MACROS_H

#ifdef UNIT_TEST
    #include <iostream>
    #define STATIC
    #define SERIAL_PRINT(X)      (std::cout << +X)
    #define SERIAL_PRINTLN(X)    (std::cout << +X << std::endl)
    #define SERIAL_PRINTLNEND()  (std::cout << std::endl)
    // note + is used to ensure that uint8_t is printed as char
#else
    #define STATIC               static
    #define DBG_PRINT(X)
    #define DBG_PRINTLN(X)
    #define SERIAL_PRINT(X)      Serial.print(X)  
    #define SERIAL_PRINTLN(X)    Serial.println(X)  
    #define SERIAL_PRINTLNEND()  Serial.println()
#endif // UNIT_TEST

#ifdef DEBUG
    #define DBG_PRINT(X)         SERIAL_PRINT(X)
    #define DBG_PRINTLN(X)       SERIAL_PRINTLN(X)
    #define DBG_PRINTLNEND()     SERIAL_PRINTLNEND()
#else
    #define DBG_PRINT(X)
    #define DBG_PRINTLN(X)
    #define DBG_PRINTLNEND()
#endif // DEBUG

#endif // MACROS_H include guard