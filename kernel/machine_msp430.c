/*
 * MSP430 specific things
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */

#if defined(__MSP430_449__)
#include <msp430x44x.h>
#elif defined(__MSP430_169__) || defined(__MSP430_1611__)
#include <msp430x16x.h>
#elif defined(__MSP430_149__)
#include <msp430x14x.h>
#else
#error unsupported
#endif
#include <legacymsp430.h>
#include <stdio.h>
#include <string.h>

#ifdef EGG_HW
#define MAX_FASTTIMERS 2
#define MAX_SLOWTIMERS 1
#else
#define MAX_FASTTIMERS 3
#define MAX_SLOWTIMERS 0
#endif

/* Use own setjmp/longjmp since some mspgcc implementations behave strange. */
int __attribute__((naked)) my_setjmp(jmp_buf env)
{
    asm(
    "mov     @r1,    0(r15)  ;0x0000(r15)\n"
    "mov     r1,     2(r15)  ;0x0002(r15)\n"
    "incd    2(r15)          ;0x0002(r15)\n"
    "mov     r2,     4(r15)  ;0x0004(r15)\n"
    "mov     r4,     6(r15)  ;0x0006(r15)\n"
    "mov     r5,     8(r15)  ;0x0008(r15)\n"
    "mov     r6,     10(r15) ;0x000a(r15)\n"
    "mov     r7,     12(r15) ;0x000c(r15)\n"
    "mov     r8,     14(r15) ;0x000e(r15)\n"
    "mov     r9,     16(r15) ;0x0010(r15)\n"
    "mov     r10,    18(r15) ;0x0012(r15)\n"
    "mov     r11,    20(r15) ;0x0014(r15)\n"
    "clr     r15\n"
    "ret\n"
    );
}
void __attribute__((naked)) __attribute__((noreturn)) my_longjmp(jmp_buf env, int val)
{
    asm(
    "mov     r15,    r13\n"
    "mov     r14,    r15\n"
    "mov     2(r13), r1      ;0x0002(r13)\n"
    "mov     4(r13), r2      ;0x0004(r13)\n"
    "mov     6(r13), r4      ;0x0006(r13)\n"
    "mov     8(r13), r5      ;0x0008(r13)\n"
    "mov     10(r13),r6      ;0x000a(r13)\n"
    "mov     12(r13),r7      ;0x000c(r13)\n"
    "mov     14(r13),r8      ;0x000e(r13)\n"
    "mov     16(r13),r9      ;0x0010(r13)\n"
    "mov     18(r13),r10     ;0x0012(r13)\n"
    "mov     20(r13),r11     ;0x0014(r13)\n"
    "br      @r13\n"
    );
}

struct timer
{
  unsigned int ticks;
  unsigned int reload;
  int (*func)(HANDLE_TIMER, void*);
  void *data;
  unsigned char enabled;
};

#if (MAX_FASTTIMERS > 0)
static struct timer ftimers[MAX_FASTTIMERS];
#endif
#if (MAX_SLOWTIMERS > 0)
static struct timer stimers[MAX_SLOWTIMERS];
#endif

HANDLE_TIMER machine_timer_create(int ival, int (*func)(HANDLE_TIMER, void*), void* data)
{
   int i, max;
   struct timer *t;

#if (MAX_FASTTIMERS > 0) && (MAX_SLOWTIMERS > 0)
   if (ival < 0) {
#endif
#if (MAX_SLOWTIMERS > 0)
     max = MAX_SLOWTIMERS;
     t = stimers;
     ival = -ival;
#endif
#if (MAX_FASTTIMERS > 0) && (MAX_SLOWTIMERS > 0)
   } else {
#endif
#if (MAX_FASTTIMERS > 0)
     max = MAX_FASTTIMERS;
     t = ftimers;
#endif
#if (MAX_FASTTIMERS > 0) && (MAX_SLOWTIMERS > 0)
   }
#endif

   for  (i=0; i<max; i++)
   {
     if (t[i].enabled == 0)
       break;
   }
   if (i==max)
     return NULL;
  
   t[i].func = func;
   t[i].data = data;
   t[i].ticks = t[i].reload = ival;
   t[i].enabled=1;
   
   return (HANDLE_TIMER)&t[i];
}

void machine_timer_destroy(HANDLE_TIMER hTimer)
{
  struct timer *timer = (struct timer*) hTimer;
  
  if (timer != NULL) {
    timer->enabled = 0;
  }
}

void machine_timer_reset(HANDLE_TIMER hTimer, int ival)
{
  dint();
  hTimer->reload = ival;
  eint();
}

static
int msp430_timer_tick(void)
{
  int i;
  int wup = 0;
  
  for (i=0; i<MAX_FASTTIMERS; i++)
  {
    if (ftimers[i].enabled)
    {
      ftimers[i].ticks--;
      if (ftimers[i].ticks==0)
      {
        wup |= ftimers[i].func((HANDLE_TIMER)&ftimers[i], ftimers[i].data);
        ftimers[i].ticks = ftimers[i].reload;
      }
    }  
  }
  return wup;
}



static rtc_time_t rtc_now, rtc_alarm;
static rtc_alarm_callback rtc_alarm_cb;
static void *rtc_alarm_data;

signed char rtc_carry(signed char *val, signed char limit)
{
  signed char carry = 0;
  
  while (*val >= limit)
  {
    *val -= limit;
    carry++;
  }
  while (*val < 0)
  {
    *val += limit;
    carry--;
  }
  return carry;
}

void rtc_add(rtc_time_t *t, signed char secs)
{
  t->seconds += secs; 
  t->minutes += rtc_carry(&t->seconds, 60);
  t->hours   += rtc_carry(&t->minutes, 60);
#ifndef USE_CALENDAR
                rtc_carry(&t->hours, 24);
#else
  t->day     += rtc_carry(&t->hours, 24);
  t->month   += rtc_carry(&t->day, 31); /* FIXME: This is not correct */
  t->year    += rtc_carry(&t->month, 12);
#endif  
}

static int rtc_time_isequal(rtc_time_t *a, rtc_time_t *b)
{
  if (a->seconds != b->seconds) return 0; 
  if (a->minutes != b->minutes) return 0; 
  if (a->hours != b->hours) return 0; 
#ifdef USE_CALENDAR
  if (a->day != b->day) return 0; 
  if (a->month != b->month) return 0; 
  if (a->year != b->year) return 0; 
#endif

  return 1;
}

int msp430_rtc_tick(void)
{
  int wup = 0;

  rtc_add(&rtc_now, 1);
  if (rtc_time_isequal(&rtc_now, &rtc_alarm))
  {
    if (rtc_alarm_cb != NULL)
      rtc_alarm_cb(rtc_alarm_data);
  }

  /* Slow timers */
#if (MAX_SLOWTIMERS > 0)
  {
    int i;

    for (i=0; i<MAX_SLOWTIMERS; i++)
    {
      if (stimers[i].enabled)
      {
        stimers[i].ticks--;
        if (stimers[i].ticks==0)
        {
          stimers[i].ticks = stimers[i].reload;
          wup |= stimers[i].func((HANDLE_TIMER)&stimers[i], stimers[i].data);
        }
      }  
    }
  }
#endif

  return wup;
}

static void rtc_init(void)
{  
  rtc_alarm_cb = NULL;
}

void rtc_setclock(rtc_time_t *t)
{
  memcpy(&rtc_now, t, sizeof(rtc_time_t));
}

void rtc_getclock(rtc_time_t *t)
{
  memcpy(t, &rtc_now, sizeof(rtc_time_t));
}
void rtc_setalarm(rtc_time_t *t, rtc_alarm_callback cb, void *data)
{
  memcpy(&rtc_alarm, t, sizeof(rtc_time_t));
  rtc_alarm_cb = cb;
  rtc_alarm_data = data;
}

void rtc_getalarm(rtc_time_t *t)
{
  memcpy(t, &rtc_alarm, sizeof(rtc_time_t));
}

#ifdef POELI_HW
static
void adc_init(void)
{
  ADC12CTL0 = 0;

  /* ADC12 sensor inputs */
  P6SEL = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7;
  P6DIR = 0;
  P6OUT = 0;

  ADC12CTL0 = SHT1_6|SHT0_6|MSC|ADC12ON;
  ADC12CTL1 = CSTARTADD_0|SHS_1|SHP|ADC12DIV_0|ADC12SSEL_0|CONSEQ_3;

  ADC12IFG = 0;

  /* Trigger interrupt for first and last ADC12MEM for data validation */
  ADC12IE = (1<<11) | (1<<0);

  /* Setup channel references and memory */
  ADC12MCTL0 = INCH_0|SREF_0;
  ADC12MCTL1 = INCH_1|SREF_0;
  ADC12MCTL2 = INCH_2|SREF_0;
  ADC12MCTL3 = INCH_3|SREF_0;
  ADC12MCTL4 = INCH_4|SREF_0;
  ADC12MCTL5 = INCH_5|SREF_0;
  ADC12MCTL6 = INCH_6|SREF_0;
  ADC12MCTL7 = INCH_7|SREF_0;
  ADC12MCTL8 = INCH_8|SREF_0;
  ADC12MCTL9 = INCH_9|SREF_0;
  ADC12MCTL10 = INCH_10|SREF_0;     /* Temp */
  ADC12MCTL11 = INCH_11|SREF_0|EOS; /* AVcc */

  ADC12CTL0 |= ENC;
}
#else

#define VREF 3300
#define VCCREF 12500

int adc_read_single(int c)
{
  unsigned int val;

  ADC12CTL1 = (ADC12CTL1 & 0x0fff) | (c<<12);
  ADC12CTL0 |= ADC12SC|ENC;
  while (ADC12CTL1 & ADC12BUSY) ;
  val = ADC12MEM[c];
  ADC12CTL0 &= ~ENC;

  /* Post process data */
  switch (c){
    /* external LM35 sensors. 10mV per Degree C. */
    case 2:
    case 3:
      /* factor 2500/4095 (one decimal place) */
      val = mult_u16xu16h(val, 40010) - 500;
      val -= 500; /* 500mV offset. Hardware specific. */
      break;
    /* external Freescale MPX4250 sensor 0-2500mBar 0.204-4.909 Volt */
    case 0:
      if (val > 167) {
        val -= 167;
      } else {
        val = 0;
      }
      /* factor = 2500/3853 */
      val = mult_u16xu16h(val, 42523);
      break;
    /* external Freescale MPX5100 sensor 0-1000mBar 0.2-4.7 Volt */
    case 1:
      if (val > 164) {
        val -= 164;
      } else {
        val = 0;
      }
      /* factor = 1000/3686 */
      val = mult_u16xu16h(val, 17780);
      break;
    /* Internal temperature */
    case 4:
      /* VTEMP=0.00355(TEMPC)+0.986, factor 10 for one decimal place */
      val = mult_u16xu16h(val<<4, 7044) - 2777;
      break;
    case 5: /* AVcc/2 */
      val = mult_u16xu16h(val, 2500<<3);
      break;
  }

  return val;
}

void adc_read(unsigned short s[], int n)
{
  int i;

  for (i=0; i<n; i++) {
    s[i] = adc_read_single(i);
  }
}

static
void adc_init(void)
{
  ADC12CTL0 = SHT1_4|SHT0_4|REFON|REF2_5V|ADC12ON;
  ADC12CTL1 = CSTARTADD_0|SHS_0|SHP|ADC12DIV_0|ADC12SSEL_0|CONSEQ_0;

  ADC12IFG = 0;

  ADC12IE = 0;

  /* Setup channel references and memory */
  ADC12MCTL0 = INCH_0|SREF_1;
  ADC12MCTL1 = INCH_1|SREF_1;
  ADC12MCTL2 = INCH_2|SREF_1;
  ADC12MCTL3 = INCH_3|SREF_1;
  ADC12MCTL4 = INCH_10|SREF_1; /* Temp */
  ADC12MCTL5 = INCH_11|SREF_1; /* AVcc */
}
#endif /* ifndef POELI_HW */

#ifdef __MSP430_449__
static void msp430_core_init(void)
{
  FLL_CTL0 &= ~XTS_FLL;         /* XT1 as low-frequency */
  FLL_CTL0 |= DCOPLUS;          /* DCO = N*ACLK */
  _BIC_SR(OSCOFF);              /* turn on XT1 oscillator */

  do                            /* wait in loop until crystal */
    IFG1 &= ~OFIFG;
  while (IFG1 & OFIFG);

  /* Set clock at 4.915200 MHz*/
  SCFQCTL = SCFQ_M | (75-1);   /* Multiplier = 75, modulation on. */
  SCFI0 = FLLD0 | FN_2;         /* Aditional *2 multiplier */

  FLL_CTL1 &= ~FLL_DIV0;        /* ACLK = XT1/1 */
  FLL_CTL1 &= ~FLL_DIV1; 

  IFG1 &= ~OFIFG;               /* clear osc. fault int. flag */
  FLL_CTL1 &= ~SELM0;           /* set DCO as MCLK */
  FLL_CTL1 &= ~SELM1;

  FLL_CTL1 &= ~SELS;            /* SMCLK = DCOCLK */
  FLL_CTL1 |= XT2OFF;           /* Turn off XT2 */
}

static void machine_init_io(void)
{
  int i;

  /* Setup RTC */
  rtc_init();

  /* LCD background light */
  P2OUT |= BIT6;
  P2SEL &= ~BIT6;
  P2DIR |= BIT6;

  /* Enable LED Output */
  P1SEL=BIT5;                                             /* p1.5 is 32768Hz */
  P1DIR=BIT5 | BIT3 | BIT0 | BIT2;                        /* BUZ,LED are outputs */
  P1OUT |= BIT3 | BIT0 | BIT2;

  /* Stay awake pin and its wake up interrupt */
  P1DIR &= ~BIT7;
  P1OUT |= BIT7;
  P1IFG &= ~BIT7;
  P1IES &= ~BIT7; /* rising edge */
  P1IE |= BIT7;

  /* ADC12 sensor inputs */
  P6SEL = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7;
  P6DIR = 0;
  P6OUT = 0;

  /* Enable basic timer interrupt */
  IE2 |= BTIE;
  
  /* Watchdog timer */
  WDTCTL = WDTPW | WDTTMSEL | WDTCNTCL | WDTSSEL;
  IE1 |= WDTIE;
}

interrupt(BASICTIMER_VECTOR) msp430_basic_timer_isr(void)
{
  int i;
  int wup = 0;

  wup = msp430_timer_tick();

  if (wup) {
    machine_wakeup();
  }
} 

interrupt(WDT_VECTOR) msp430_watchdog_rtc(void)
{
  int wup;

  wup = msp430_rtc_tick();

  /* If supposed to sleep, lets see if we should wake up */
  if ((P3IN & 0xf0) != 0xf0) {
    wup = 1;
  }

  if (wup)
    machine_wakeup();
}

#elif defined(__MSP430_169__) || defined(__MSP430_1611__) || defined(__MSP430_149__)

static void msp430_core_init(void)
{
  /* MCLK = DCO @ ~8MHz, SMCLK = MCLK */
  DCOCTL = DCO2|DCO1|DCO0;
  BCSCTL1 = XT2OFF|DIVA_0|RSEL2|RSEL1|RSEL0;
  BCSCTL2 = SELM_0|DIVM_0|DIVS_0;
}

#ifdef POELI_HW
void machine_init_io(void)
{
  /* TimerB PWM outputs */
  P4OUT = BIT5|BIT6;
  P4DIR = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7;
  P4SEL = 0;

  /* actuator diagnostic inputs */
  P2OUT = 0;
  P2DIR = BIT6|BIT7;
  P2SEL = 0;

  /* P_ACK and W-Bus I/O */
  P3OUT = BIT4|BIT6;
  P3DIR = BIT3|BIT4|BIT6;
  /* Note: select peripheral pins on demand, to use them as GPIO as well. */
  /* P3SEL = BIT4|BIT5|BIT6|BIT7; */

  /* */
  P1OUT = 0;
#ifdef CANBUS_ENABLE
  P1DIR = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT7;
#else
  P1DIR = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7;
#endif
  P1SEL = 0;

  /* CAN-BUS */
#ifdef CANBUS_ENABLE
  P5SEL = BIT0|BIT1|BIT2|BIT3;
  P5DIR = BIT0|BIT1|BIT3|BIT4|BIT5|BIT6|BIT7;
  P5OUT = 0;
#else
  P5SEL = 0;
  P5DIR = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7;
  P5OUT = 0;
#endif

  /* disable any peripheral which is not always on */
  TBCTL = 0;
  U0CTL = 0;
  U1CTL = 0;
}

/* Timer A OUT1 toggles ADC 12 trigger signal, thus a factor 2 in the periods. */
#define ADCTRIG_SLOW (JFREQ*5)
#define ADCTRIG_FAST (JFREQ/20)
static unsigned int adc12period;

void machine_init_timer(void)
{
  /* Timer A as main timer source and ADC12 trigger */
  TACTL = 0;
  TACCTL0 = OUTMOD_0|CCIE; /* RTC */
  TACCTL1 = OUTMOD_4|CCIE; /* ADC12 trigger */
  TACCTL2 = OUTMOD_0|CCIE; /* Timer tick */
  /* Timer A clock 32768Hz / 8 = 4096Hz (JFREQ) */
  TACCR0 = JFREQ; /* RTC 1Hz time base */
  adc12period = ADCTRIG_SLOW;
  TACCR1 = adc12period;
  TACTL = TASSEL_1|ID_3|MC_2|TACLR;
}

void adc_interval_set(int fast)
{
  if (fast) {
    adc12period = ADCTRIG_FAST;
    TACCR1 = TAR + adc12period;
  } else {
    adc12period = ADCTRIG_SLOW;
    TACCR1 = TAR + adc12period;
  }
}

#elif defined(EGG_HW)

void machine_init_io(void)
{
  /* DOGM162 data */
  P4OUT = 0x00;
  P4DIR = 0xff;
  P4SEL = 0x00;

  /* DOGM162 ctrl */
  P5OUT = 0x00;
  P5DIR |= BIT0|BIT1|BIT2;
  P5SEL &= ~(BIT0|BIT1|BIT2);

  /* LCD background light, GSM on/off, GSM power inhibit */
  P6OUT &= ~(BIT3|BIT4|BIT2);
  P6SEL &= ~(BIT3|BIT4|BIT2);
  P6DIR |= BIT3|BIT4|BIT2;

  /* Buttons */
  P3DIR &= ~(BIT0|BIT1|BIT2|BIT3);
  P3OUT |= BIT0|BIT1|BIT2|BIT3;
  P3SEL &= ~(BIT0|BIT1|BIT2|BIT3);

  /* disable any peripheral which is not always on */
  TBCTL = 0;
  U0CTL = 0;
  U1CTL = 0;
}

void machine_init_timer(void)
{
  /* Timer A as main timer source */
  TACTL = 0;   
  TACCTL0 = OUTMOD_0|CCIE;
  TACCTL1 = 0;
  TACCTL2 = OUTMOD_0|CCIE;
  /* Timer A clock 32768Hz / 8 = 4096Hz (JFREQ) */
  TACCR0 = JFREQ; /* RTC 1Hz time base */
  TACCR2 = JFREQ/TFREQ;
  TACTL = TASSEL_1|ID_3|MC_2|TACLR;
}

#define BACKLIGHT_PWM

#ifdef BACKLIGHT_PWM
static unsigned int machine_backlight_pwm0, machine_backlight_pwm1;
#endif

#else

#error unknown hardware variant

#endif

interrupt(TIMERA0_VECTOR) msp430_timer_a0_isr(void)
{
  int wup;
  /* 1Hz interval */
  TACCR0 = TACCR0 + JFREQ;

  wup = msp430_rtc_tick();
  if (wup) {
    machine_wakeup();
  }
}

interrupt(TIMERA1_VECTOR) msp430_timer_a1_isr(void)
{
  int wup=0;
 	
  switch (TAIV) {
#ifdef POELI_HW
#if (__MSPGCC__ >= 20110706)
    case TAIV_TACCR1:
#else
    case TAIV_CCR1:
#endif
      TACCR1 = TACCR1 + adc12period;   /* variable trigger for ADC12 */
      break;
#endif
#ifdef BACKLIGHT_PWM
#if (__MSPGCC__ >= 20110706)
    case TAIV_TACCR1:
#else
    case TAIV_CCR1: 
#endif
      /* variable backlight PWM */
      if (P6OUT & BIT3) {
        TACCR1 = TACCR1 + machine_backlight_pwm0;
        P6OUT &= ~BIT3;
      } else {
        TACCR1 = TACCR1 + machine_backlight_pwm1;
        P6OUT |= BIT3;
      }
      break;
#endif
#if (__MSPGCC__ >= 20110706 )
    case TAIV_TACCR2:
#else
    case TAIV_CCR2:
#endif
      TACCR2 = TACCR2 + (JFREQ/TFREQ); /* software timer base */
      wup = msp430_timer_tick();
      break;
  }
  if (wup) {
    machine_wakeup();
  }
} 

#endif /* MSP430 type */

void machine_init(void)
{
  int i;
  
  WDTCTL = WDTPW | WDTHOLD;     /* stop watchdog timer */
  
  /* Setup CPU clock */
  msp430_core_init();

  /* Setup RTC */
  rtc_init();

  machine_init_io();

  machine_init_timer();

  /* Setup adc for sensor reading */
  adc_init();

  /* Set all timers disabled */
#if (MAX_FASTTIMERS > 0)
  for (i=0; i<MAX_FASTTIMERS; i++) {
    ftimers[i].enabled = 0;
  }
#endif
#if (MAX_SLOWTIMERS > 0)
  for (i=0; i<MAX_SLOWTIMERS; i++) {
    stimers[i].enabled = 0;
  }
#endif

  /* global interrupt enable */
  eint();
}
  

void machine_usleep(unsigned long us) /* 9+a*12 cycles */
{
  unsigned long tstart;
  
  tstart = machine_getJiffies();
  
  /* What a luxury we can just divide :) */
  us = us / (1000000/JFREQ);
  while ( (machine_getJiffies() - tstart) < us) ;
}

static HANDLE_TIMER hSleepTimer = NULL;

static
int sleep_timer(HANDLE_TIMER t, void *data)
{
  return 1;
}

void machine_jsleep(unsigned int j)
{
  j = JIFFIES2TIMER(j);

  if (j==0) {
    return;
  }
  hSleepTimer = machine_timer_create(j, sleep_timer, NULL);
  machine_suspend();
  machine_timer_destroy(hSleepTimer);
}

void machine_msleep(unsigned int b)
{
  machine_jsleep(b*(JFREQ/1000));
}

void machine_sleep(unsigned int b)
{
  unsigned int m;
  for(m=0;m!=b;m++) machine_msleep(1000);
}

#ifdef EGG_HW

#ifdef __MSP430_449__

#define          BUZ1_ON            P1OUT |= BIT0     //P4.2
#define          BUZ1_OFF           P1OUT &= ~BIT0    //P4.2
#define          BUZ2_ON            P1OUT |= BIT2     //P4.3
#define          BUZ2_OFF           P1OUT &= ~BIT2    //P4.3

void machine_beep(void)
{
  unsigned int i;

  for (i=0; i<110; i++)
  {
    BUZ1_OFF;
    BUZ2_ON; 
    machine_usleep(300);
    BUZ2_OFF;
    BUZ1_ON;
    machine_usleep(300);            
  }
}

int machine_backlight_get(void)
{
  return (P2OUT & BIT6) ? 0 : 1;
}

void machine_backlight_set(int en)
{
  if (en) {
    P2OUT &= ~BIT6;
  } else {
    P2OUT |= BIT6;
  }
}

unsigned int machine_buttons(int do_read)
{
  return (P3IN>>4)^0x0f;
}

void machine_led_set(int s)
{
  if (s)
    P1OUT &= ~BIT3;
  else
    P1OUT |= BIT3;
}

#elif defined(__MSP430_169__) || defined(__MSP430_1611__) || defined(__MSP430_149__)

void machine_beep(void)
{
}

unsigned int machine_buttons(int do_read)
{
  return (P3IN & 0x0f)^0x0f;
}

int machine_backlight_get(void)
{
#ifdef BACKLIGHT_PWM
  return machine_backlight_pwm1-1;
#else
  return (P6OUT & BIT3) ? BACKLIGHT_MAX : 0;
#endif
}

void machine_backlight_set(int en)
{
#ifdef BACKLIGHT_PWM
  machine_backlight_pwm0 = (BACKLIGHT_MAX-en)+2;
  machine_backlight_pwm1 = en+1;
  if (en > 0) {
    TACCTL1 = OUTMOD_0|CCIE;
    TACCR1 = TAR + machine_backlight_pwm0;
  } else {
    TACCTL1 = 0;
    P6OUT &= ~BIT3;
  }
#else
  if (en) {
    P6OUT |= BIT3;
  } else {
    P6OUT &= ~BIT3;
  }
#endif
}

void machine_led_set(int s)
{

}

#else

#error unspported ARCH

#endif

#elif POELI_HW /* Hardware variant */

unsigned int machine_buttons(int do_read)
{
#ifdef POELI_UART1_ON
  /* Treat UART 1 line as "button", active low */
  return (P3IN & BIT7) ? 0 : 1;
#else
  return 0;
#endif
}

void machine_led_set(int s)
{
  if (s)
    P1OUT |= BIT3;
  else
    P1OUT &= ~BIT3;
}

#else /* Hardware variant */

#error Unsupported VARIANT

#endif /* Hardware variant */


unsigned int machine_getJiffies(void)
{
#ifdef __MSP430_449__
  return (BTCNT2<<8)|BTCNT1;
#else
  return TAR;
#endif
}

#ifdef EGG_HW

#ifdef __MSP430_449__
interrupt(PORT1_VECTOR) msp430_port1_interrupt(void)
{
  P1IFG = 0;
  machine_wakeup();
}
#endif

int machine_stayAwake(void)
{
  int result;

#ifdef __MSP430_449__
  result = ((P3IN & 0xf0) != 0xf0) || (P1IN & BIT7));
#else
  result = (P3IN & 0x0f) != 0x0f;
#endif

  return result;
}
#elif defined(POELI_HW)
int machine_stayAwake(void)
{
  return 1;
}
#endif

/*
 * Put the msp430 to sleep in lowest power mode
 */
void machine_suspend(void)
{

  /* Go to sleep */
  LPM0;

}

#if 0

typedef void(block_write_t)(void*, void *, int);

void flash_block(int *_fptr, int *_rptr, int words)
{
  FCTL3 = FWKEY;          /* Unlock */
  FCTL1 = FWKEY | BLKWRT;

  for (; words>0; words--) {
    if (_rptr != NULL) {
      *_fptr++ = *_rptr++;
    } else {
      *_fptr++ = 0;
    }
    /* wait until write is ready */
    while (FCTL3 & WAIT) ;
  }

  while (FCTL3 & BUSY) ;
}
void flash_block_end(void) { ; }

#define FCN_SIZE ((size_t)flash_block_end-(size_t)flash_block)

void flash_write(void *_fptr, void *_rptr, int nbytes)
{
  int wdg;
  int *fptr = _fptr;
  int *rptr = _rptr;
  int words = ((nbytes & 63)+1)>>1;
  unsigned int p_block_write[sizeof(FCN_SIZE)/sizeof(short)];
  unsigned int mask;
  block_write_t *pf_block_write = (block_write_t*)p_block_write;

  memcpy(p_block_write, flash_block, FCN_SIZE);

  /* Disable interrupts and watchdog */
  dint();
  wdg = WDTCTL & 0x00ff;
  WDTCTL=WDTPW|WDTHOLD;

  while (FCTL3 & BUSY) ;

  /* Setup flash */
#ifdef __MSP430_449__
  FCTL2 = FWKEY | FSSEL_2 | (16-1);   /* Use 4.91MHz SMCLK / 16 = 300kHz */
#else
  FCTL2 = FWKEY | FSSEL_1 | (17-1);   /* Use 8.00MHz MCLK / 17 = 470.588kHz */
#endif
  if (isInfoMem) {
    mask = 0x0ff;
  } else {
    mask = 0x1ff;
  }

  /* Write new data */  
  for (; words>0; words-=32) {
    if ( ((size_t)fptr & mask) == 0) { 
      /* Erase segment */
      FCTL3 = FWKEY;          /* Unlock */
      FCTL1 = FWKEY | ERASE;  /* Erase segment */
      *fptr = 0xff;
    }
    pf_block_write(fptr, rptr, (words > 32) ? 32 : words);
    fptr += 32;
    rptr += 32;
  }

  FCTL3 = FWKEY|LOCK;     /* Lock */
  FCTL1 = FWKEY;

  /* Restore Watchdog settings */
  WDTCTL = wdg | WDTPW;
 /* Enable interrupts */
  eint();
}

#else

void flash_write(void *_fptr, void *_rptr, int nbytes)
{
  int wdg;
  int *fptr = _fptr;
  int *rptr = _rptr;
  int words = (nbytes+1)>>1, i;

  /* Disable interrupts and watchdog */
  dint();
  wdg = WDTCTL & 0x00ff;
  WDTCTL=WDTPW|WDTHOLD;

  /* Setup flash */
#ifdef __MSP430_449__
  FCTL2 = FWKEY | FSSEL_2 | (16-1);   /* Use 4.91MHz SMCLK / 16 = 300kHz */
#else
  FCTL2 = FWKEY | FSSEL_1 | (17-1);   /* Use 8.00MHz MCLK / 17 = 470.588kHz */
#endif

  /* Erase segment */
  FCTL3 = FWKEY;          /* Unlock */
  FCTL1 = FWKEY | ERASE;  /* Erase segment */
  *fptr = 0xff;

  FCTL3 = FWKEY | LOCK;   /* Lock */

  /* Write new data */
  if (rptr != NULL) {
    for (i=0; i<words; i++)
    {
      FCTL3 = FWKEY;          /* Unlock */
      FCTL1 = FWKEY | WRT;
      *fptr++ = *rptr++;
      FCTL1 = FWKEY;
      FCTL3 = FWKEY|LOCK;     /* Lock */
    }
  } else {
    for (i=0; i<words; i++)
    {
      FCTL3 = FWKEY;          /* Unlock */
      FCTL1 = FWKEY | WRT;
      *fptr++ = 0;
      FCTL1 = FWKEY;
      FCTL3 = FWKEY|LOCK;     /* Lock */
    }
  }

  /* Reset timers to avoid compare register reload trouble. */
  machine_init_timer();
#ifdef BACKLIGHT_PWM
  machine_backlight_set(machine_backlight_get());
#endif

  /* restore watchdog and interrupts */
  WDTCTL = wdg | WDTPW;
  eint();
}
#endif
