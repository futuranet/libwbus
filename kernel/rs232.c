/*
 * Junction file for different rs232 backend implementations.
 */

#include "rs232.h"

#if    defined(__MSP430_449__) || defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
#include "rs232_msp430.c"
#elif  defined(__arm__)
#include "rs232_arm.c"
#elif  defined(__linux__) || defined(__Cygwin__)
#include "rs232_posix.c"
#elif  defined(_WIN32)
#include "rs232_win32.c"
#else
#error Unsupported machine, sorry.
#endif
