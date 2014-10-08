
/*
 * Machine specific things
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */
#ifndef __MACHINE_H__
#define __MACHINE_H__

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

#if defined(__MSP430__) && (!defined(__MSP430_149__) && !defined(__MSP430_169__) && !defined(__MSP430_1161__) && !defined(__MSP430_449__))
#error Unsupported hardware
#endif

/* Hardware description */
#if !defined(CAF_TYPE) || (CAF_TYPE + 0 == 0)
#error "CAF_TYPE undefined. 1: Toy, 2: DBW46"
#elif (CAF_TYPE == 1)
#define CF_TOYBLOWER
#elif (CAF_TYPE == 2)
#define CF_DBW46
#else
#error "CAF_TYPE unsupported"
#endif

#if !defined(PSENSOR) || (PSENSOR + 0 == 0)
#error "PSENSOR undefined. 1: MPX5100, 2: MPX4250"
#elif (PSENSOR == 1)
#define PSENSOR_MPX5100
#elif (PSENSOR == 2)
#define PSENSOR_MPX4250
#else
#error "PSENSOR unsupported."
#endif

#ifdef __linux__
#include <stdio.h>
#define HAS_PRINTF
#define PRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define PRINTF(fmt, ...) { ; }
#endif

void machine_init(void);

void machine_usleep(unsigned long);
void machine_msleep(unsigned int);
void machine_sleep(unsigned int);
void machine_jsleep(unsigned int);

/* Timer support */

typedef struct timer *HANDLE_TIMER;
typedef int (*timer_func)(HANDLE_TIMER, void*);

/*
 * Register a timer.
 * \param ival period of timer in ticks - if > 0 running at 16384 Hz if < 0 running at 1Hz
 *    if ival < 0 the timer does not stop in low power mode. See machine_suspend().
 * \param func callback function pointer
 * \param data arbitrary data pointer passed to the callback
 * \return timer handle if success or NULL if failure
 * If the timer call back returns !=0 then the uC is waked up. 
 */
HANDLE_TIMER machine_timer_create(int ival, timer_func func, void *data);
/*
 * Stop and free a given timer
 */
void machine_timer_destroy(HANDLE_TIMER);
/*
 * Change interval of timer
 */
void machine_timer_reset(HANDLE_TIMER hTimer, int ival);

void machine_beep(void);
void machine_led_set(int s);
int machine_backlight_get(void);
void machine_backlight_set(int en);
unsigned int machine_buttons(int do_read);
void machine_lcd(int enable);

/* Jiffies clocked at 16384 Hz */
#if defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
#define TFREQ 128UL /* Software timer timebase */
#define JFREQ 4096UL /* Jiffies timebase */
#define JIFFIES2TIMER(x) ((x)>>5) 
#define TIMER2JIFFIES(x) ((x)<<5)
#define POELI_UART1_ON
#elif defined(__MSP430_449__)
#define TFREQ 128UL /* Software timer timebase */
#define JFREQ 32768UL /* Jiffies timebase */
#define JIFFIES2TIMER(x) ((x)>>8)
#define TIMER2JIFFIES(x) ((x)<<8)
#elif defined(__linux__)
#include <time.h>

#define TFREQ 100
#define JFREQ 100
#elif defined(_WIN32)
#define TFREQ 1000
#define JFREQ 1000
#elif defined(__arm__)
#define TFREQ 1000
#define JFREQ 1000

#else
#error Sorry, unsupported platform
#endif

/* PWM frequency = SMCLK/((1<<ACTMAX_LD2)*100) */
/* Note: Some constants might overflow if ACTMAX_LD2 is increased. Also
   PWM frequency will be reduced by increasing ACTMAX_LD2 (hardware limitation). */
#define ACTMAX_LD2 2
#define ACTMAX (100<<ACTMAX_LD2)

/* Avoid using this macros with variables; division is slow. */
#define MSEC2JIFFIES(x) ((JFREQ*(long)(x))/1000)
#define USEC2JIFFIES(x) ((JFREQ*(long)(x))/1000000)
#define MSEC2TIMER(x) ((TFREQ*(long)(x))/1000)
unsigned int machine_getJiffies(void);

/* ADC12 */

/*
 * Read single ADC channel number "c".
 */
int adc_read_single(int c);

/*
 * Read "n" ADC channels
 */
void adc_read(unsigned short s[], int n);

/**
 * Tag current ADC value set as invalid
 */
void adc_invalidate(void);

/**
 * Check if ADC value set as is already up to date again,
 * after the values were invalidated by calling adc_invalidate().
 */
int adc_is_uptodate(void);

/* Set new actuator values */
void machine_act(unsigned short a[], int n);

/* (de)activate actuators and sensors */
void machine_ack(int ack);

/* Basic power management */
void machine_suspend(void);
#if defined(__MSP430_449__)
#include <msp430x44x.h>
#elif defined(__MSP430_149__)
#include <msp430x14x.h>
#elif defined(__MSP430_169__) || defined(__MSP430_1611__)
#include <msp430x16x.h>
#else
#define LPM0_EXIT
#endif

#define machine_wakeup(x) LPM0_EXIT


int machine_stayAwake(void);

/* Realtime clock */
typedef struct {  
  signed char seconds;
  signed char minutes;
  signed char hours;
#ifdef USE_CALENDAR
  signed char day;
  signed char month;
  signed char year;
#endif
} rtc_time_t;
 
typedef void(*rtc_alarm_callback)(void *data);   

void rtc_add(rtc_time_t *t, signed char secs);
void rtc_setalarm(rtc_time_t *t, rtc_alarm_callback cb, void *data);
void rtc_setclock(rtc_time_t *t);
void rtc_getalarm(rtc_time_t *t);
void rtc_getclock(rtc_time_t *t);

/**
 * if nbytes==0, erase page of fptr and leave uninitialized.
 * if nbytes!=0 and rptr==NULL, erase and initialze nbytes bytes with zero at fptr.
 * if nbytes!=0 and rptr!=NULL, erase and write nbytes data bytes from rptr into fptr.
 */
void flash_write(void *fptr, void *rptr, int nbytes);

#ifdef __i386__

#include <setjmp.h>
#define my_setjmp(a) setjmp(a)
#define my_longjmp(a,b) longjmp(a,b)

#define setup_task(stack, function) \
    asm ( \
        "movl %0, %%esp\n" \
        "pushl %1;\n" \
        "ret;\n" \
        : : "r"(stack), "r"(function) \
    )

static inline unsigned int mult_u16xu16h(const unsigned int a, const unsigned int b)
{
  return (a*b)>>16;
}

static inline signed int mult_s16xs16h(const signed int a, const signed int b)
{
  return (a*b)>>16;
}

static inline int mac_s16xs16_h3(const signed int a1, 
                                 const signed int b1,
                                 const signed int a2,
                                 const signed int b2,
                                 const signed int a3,
                                 const signed int b3)
{
  return (a1*b1 + a2*b2 + a3*b3)>>16;
}

static inline int div_u32_u16(long dividend, int divisor)
{
  if (divisor == 0) {
    divisor = 1;
  }
  return dividend/divisor;
}

#elif defined(__arm__)

#include <setjmp.h>
#define my_setjmp(a) setjmp(a)
#define my_longjmp(a,b) longjmp(a,b)

#define setup_task(stack, function) \
    asm ( \
        "mov %%sp, %0\n" \
        "bx %1;\n" \
        : : "r"(stack), "r"(function) \
    )

#elif defined(__MSP430__)

#include <sys/types.h>
#include <setjmp.h>

int __attribute__((naked)) my_setjmp (jmp_buf env);
void __attribute__((naked)) __attribute__((noreturn)) my_longjmp (jmp_buf env, int val);

#define setup_task(stack, function) \
    asm ( "mov %0, r1;\n" \
          "br @%1;\n" \
      : : "r"(stack), "r"(&function) \
    )

/* fixpoint fractional unsigned multiplication */
static inline unsigned int mult_u16xu16h(const unsigned int a, const unsigned int b)
{
  unsigned int result;
 
  asm("push r2;\n"
      "dint;\n"
      "nop;\n"
      "mov %1, &0x0130;\n"
      "mov %2, &0x0138;\n"
      "mov &0x013c, %0;\n"
      "pop r2;\n"
      : "=r"(result)
      : "r"(a), "r"(b)
  );
  return result;
}

static inline signed int mult_s16xs16h(const signed int a, const signed int b)
{
  unsigned int result;
 
  asm("push r2;\n"
      "dint;\n"
      "nop;\n"
      "mov %1, &0x0132;\n"
      "mov %2, &0x0138;\n"
      "mov &0x013c, %0;\n"
      "pop r2;\n"
      : "=r"(result)
      : "r"(a), "r"(b)
  );
  return result;
}

static inline int mac_s16xs16_h3(const signed int a1, const signed int b1, const signed int a2, const signed int b2, const signed int a3, const signed int b3)
{
  int result;
 
  asm("push r2;\n"
      "dint;\n"
      "nop;\n"
      "mov %1, &0x0132;\n"
      "mov %2, &0x0138;\n"
      "mov %3, &0x0136;\n"
      "mov %4, &0x0138;\n"
      "mov %5, &0x0136;\n"
      "mov %6, &0x0138;\n"
      "mov &0x013c, %0;\n"
      "pop r2;\n"
      : "=r"(result)
      : "r"(a1), "r"(b1), "r"(a2), "r"(b2), "r"(a3), "r"(b3)
  );
  return result;
}

static inline unsigned int mac_u16xu16_h3(const signed int a1, const signed int b1, const signed int a2, const signed int b2, const signed int a3, const signed int b3)
{
  unsigned int result;
 
  asm("push r2;\n"
      "dint;\n"
      "nop;\n"
      "mov %1, &0x0130;\n"
      "mov %2, &0x0138;\n"
      "mov %3, &0x0134;\n"
      "mov %4, &0x0138;\n"
      "mov %5, &0x0134;\n"
      "mov %6, &0x0138;\n"
      "mov &0x013c, %0;\n"
      "pop r2;\n"
      : "=r"(result)
      : "r"(a1), "r"(b1), "r"(a2), "r"(b2), "r"(a3), "r"(b3)
  );
  return result;
}

static inline int div_u32_u16(long dividend, int divisor)
{
  int result=0;
  int i=17;

  /* This routine was taken from TI document slaa024.pdf
   *  Original author : Mr Leipold/L&G.
   */
  asm("DIV1_%=: cmp %[IROP1],%[IROP2M];\n"
      "         jlo DIV2_%=;\n"
      "         sub %[IROP1],%[IROP2M];\n"
      "DIV2_%=: RLC %[IRACL];\n"
      "         jc DIV4_%= ;\n"      // Error: result > 16 bits
      "         dec %[IRBT] ;\n"     // Decrement loop counter
      "         jz DIV3_%= ;\n"      // Is 0: terminate w/o error
      "         rla %[IROP2L];\n"
      "         rlc %[IROP2M];\n"
      "         jnc DIV1_%=;\n"
      "         sub %[IROP1],%[IROP2M];\n"
      "         setc;\n"
      "         jmp DIV2_%=;\n"
      "DIV3_%=: clrc;\n"   //No error, C = 0"
      "DIV4_%=:"
     : [IRACL]"+r"(result), [IRBT]"+r"(i)
     : [IROP1]"r"(divisor), [IROP2M]"r"(dividend>>16), [IROP2L]"r"(dividend&0xffff)
  );
  return result;
}

#elif defined(some_other_architecture)

#endif /* hardware specific stuff */


#endif /* __MACHINE_H__ */
