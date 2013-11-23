#include "machine.h"

#if    defined(__MSP430_449__) || defined (__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
#include "machine_msp430.c"
#elif  defined(__arm__)
#include "machine_arm.c"
#elif  defined(__linux__) || defined(__Cygwin__)
#include "machine_posix.c"
#elif  defined(_WIN32)
#include "machine_win32.c"
#else
#error Unsupported machine, sorry.
#endif
