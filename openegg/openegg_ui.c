/*
 * OpenEgg User Interface support. Currently PC simulation only.
 *
 * Author: Manuel Jander
 * License: BSD License
 *
 */

#include "openegg_ui.h"

#ifdef __linux__

#include "openegg_ui_posix.c"

#elif defined(__MSP430__)

#include "openegg_ui_msp430.c"

#elif defined(_WIN32)

#include "openegg_ui_win32.c"

#endif
