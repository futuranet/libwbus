#include "poeli_ctrl.h"

#if    defined(__MSP430_449__) || defined (__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
#include "poeli_ctrl_msp430.c"
#else
#include "poeli_ctrl_posix.c"
#endif
