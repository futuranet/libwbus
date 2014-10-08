/*
 * Actuator and sensor control
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */

#ifndef POELI_CTRL_H
#define POELI_CTRL_H

/* Work around backward compatibility. */
#if defined(__MSP430F149__) || defined(__MSP430X149__)
#define __MSP430_149__ 
#elif defined(__MSP430F169__) || defined(__MSP430X169__)
#define __MSP430_169__
#elif defined(__MSP430F1161__) || defined(__MSP430X1161__)
#define __MSP430_1161__
#elif defined(__MSP430F449__) || defined(__MSP430X449__)
#define __MSP430_449__
#endif

void poeli_ctrl_read(unsigned short s[], int n);

void poeli_ctrl_init(void);

#endif
