#ifndef MACROS_H
#define MACROS_H

#ifdef UNIT_TEST
    #define STATIC
#else
    #define STATIC static
#endif // UNIT_TEST

#endif // MACROS_H include guard