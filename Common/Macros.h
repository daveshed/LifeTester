#ifndef MACROS_H
#define MACROS_H

#ifdef UNIT_TEST
    #define STATIC
#else
    STATIC static
#endif // UNIT_TEST

#endif // MACROS_H include guard