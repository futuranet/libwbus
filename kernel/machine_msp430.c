/*
 * MSP430 specific things
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */

#include "machine.h"

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

#ifdef __MSP430_449__
#define MAX_FASTTIMERS 2
#define MAX_SLOWTIMERS 1
#else
#define MAX_FASTTIMERS 3
#define MAX_SLOWTIMERS 0
#endif

#define CCPID_ENABLE
#define CFPID_ENABLE

/* Modulate GK and Nozzle stock preheating power */
//#define GK_MODULATE

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

//#define USE_RPWM

#ifdef USE_RPWM
#include <stdlib.h>
#define RPWM_BITS 3
#define ACT2PWM(x) ((ACTMAX+(1<<RPWM_BITS)-(x)))

#else /* USE_RPWM */

#define ACT2PWM(x) ((ACTMAX-(x)))

#endif /* USE_RPWM */

/* Actuator diagnostic bits on port P2IN */
#define DIAG_VF  BIT0
#define DIAG_CP  BIT1
#define DIAG_AUX BIT2
#define DIAG_DP  BIT3
#define DIAG_CF  BIT4
#define DIAG_CC  BIT5

/* Actuator ouput pin state while driver on or off. */
unsigned char gP2StateOff, gP2StateOn;

interrupt(TIMERB0_VECTOR) msp430_timer_b0_isr(void)
{
#ifdef USE_RPWM
  TBCCR0 = ACTMAX-1+(rand()>>(16-RPWM_BITS));
#endif

  gP2StateOn = P2IN;
}

interrupt(TIMERB1_VECTOR) msp430_timer_b1_isr(void)
{
  unsigned int iv;
  unsigned char p2;

  iv = TBIV;
  p2 = P2IN;

#ifdef USE_RPWM
  if (iv == 0x0e) {
    TBCCR0 = ACTMAX-1+(rand()>>(16-RPWM_BITS));
  }
#endif

#if defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
  switch (iv) {
  #if 0
    case 4:  if (p2& DIAG_CP) gP2StateOff |= DIAG_CP; else gP2StateOff &= ~DIAG_CP; break; /* CP */
  #endif
    case 12: if (p2& DIAG_CC) gP2StateOff |= DIAG_CC; else gP2StateOff &= ~DIAG_CC; break; /* CC */
    case 14: if (p2& DIAG_CF) gP2StateOff |= DIAG_CF; else gP2StateOff &= ~DIAG_CF; break; /* CF */
  }
#endif
}

#if defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)

static unsigned int msp430_check_actuators(void)
{
  unsigned int result;
  
  result  =  gP2StateOff & 0x3f; 
  result |= ~gP2StateOn  & 0x3f; 

  return result;
}

static unsigned char gADCstatus=0;

interrupt(ADC12_VECTOR) msp430_adc12_interrupt(void)
{
  unsigned int ifg;

  ifg = ADC12IFG;

  /* ADCMEM0 */
  if (ifg & (1<<0)) {
    if (gADCstatus == 1) {
      gADCstatus |= 2;
    }
  }
  /* ADCMEM11 */
  if (ifg & (1<<11)) {
    if (gADCstatus == 3) {
      gADCstatus |= 4;
    }
  }

  /* Reset all flags */
  ADC12IFG = 0;
}

static unsigned int gCafPeriod = 0, gCafTimePrev = 0;
static unsigned int gCafCounter = 0;

interrupt(PORT1_VECTOR) msp430_p1(void)
{
  if (P1IFG & BIT5)
  {
    int temp;

    /* reset interrupt flag */
    P1IFG &= ~BIT5;

    /* calc time period */
    temp = machine_getJiffies();
    gCafPeriod = temp - gCafTimePrev;
    gCafTimePrev = temp;
    gCafCounter = 0;
  }
}

void adc_invalidate(void)
{
  gADCstatus = 0;
}

int adc_is_uptodate(void)
{
  /* return true if actuator were updated and ADCMEM0 through ADCMEM11 are up to date */
  return ( gADCstatus == 7 );
}
#endif

#if 0
  SENSOR_FD = 0,    /* Flame detector Seebeck voltage*/
  SENSOR_T0,        /* Temperature */ 
  SENSOR_T1,        /* Temperature */ 
  SENSOR_GKPH,      /* GKPH current */
  SENSOR_GKZ,       /* PKZ  current */
  SENSOR_P,         /* compressor air pressure for syphon nozzle */
  SENSOR_OVERHEAT,  /* Overheating fuse */
  SENSOR_VCC,       /* Power supply voltage */
  SENSOR_HE,        /* Heating energy */
  SENSOR_GPR,       /* Glow plug resistance. But I have Seebeck voltage, huh ? */
#endif


#define VREF 3300
#define VCCREF 12500

#ifndef __MSP430_449__

/* Timer A OUT1 toggles ADC 12 trigger signal, thus a factor 2 in the periods. */
#define ADCTRIG_SLOW (JFREQ*5)
#define ADCTRIG_FAST (JFREQ/20)
static unsigned int adc12period;

#ifdef GK_MODULATE
static unsigned int gkphSetPoint;
static unsigned int gkzSetPoint;
#define GK_SCALE 5
#endif

#endif /* ! __MSP430_449__ */

int adc_read_single(int c)
{
  unsigned int val;

#ifdef __MSP430_449__
  ADC12CTL1 = (ADC12CTL1 & 0x0fff) | (c<<12);
  ADC12CTL0 |= ADC12SC|ENC;
  while (ADC12CTL1 & ADC12BUSY) ;
  val = ADC12MEM[c];
  ADC12CTL0 &= ~ENC;
#else
  val = ADC12MEM[c];
#endif

#if defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
  switch (c) {
    case 0: /* Seebeck 2500mV - 1000mV / 0-30mV (inverting) */
      //val = (4095-val)*(3300/4095);
      val = mult_u16xu16h(4095-val, (VREF<<4));
      break;
    /* for details on the temperature sensors see temp_sensor.xls */
    /* value 50 equals 0 degrees celcius, 60 means 10 degress celcius. */
    case 1: /* Webasto DBW 46 heat exchanger temperature sensor 0.1468336922481	-277.909809179042 */
      if (val < (int)(((278L - 50L)*65536L)/9623L) ) {
        val = 0;
      } else {
        val = mult_u16xu16h(val, 9623) - 278 + 50;
      }
      break;
    case 2: /* KTY81-110 sensor -245.749538150444 0.13509915053347 */
      if (val < (int)(((246L - 50L)*65536L)/8854L) ) {
        val = 0;
      } else {            
        val = mult_u16xu16h(val, 8854) - 246 + 50;
      }
      break;
    case 4:
#ifdef GK_MODULATE
      if (gkzSetPoint == 0) {
        val = 0; /* Avoid invalid reads because of voltage measurement net (SENSOR_FD). */
      }
#endif
    case 3: /* GKI   Igk(mA) = ((ADCgk/4095)*3300) * 30000/Ris, Ris = 1k */
      //val = val * ((VREF/1000/4095)*(30000/1000)*100/16);
      val = mult_u16xu16h(val<<4, 9902); /* scaled by factor 10 */
      break;
    case 5: /* P Vp/2   Vp : 204mV - 4909mV / 0mBar - 2500mBar (scaled to 1Bar range) */
      /* Remove offset */
      if (val > 126) {
        val -= 126;
      } else {
        val = 0;
      }
      /* match ACTMAX to 1000 mBar. See cc.xls for details */
#if defined(PSENSOR_MPX4250)
      val = mult_u16xu16h(val, 5611<<ACTMAX_LD2 ); /* MPX4250 */
#elif defined(PSENSOR_MPX5100)
      val = mult_u16xu16h(val, 2348<<ACTMAX_LD2); /* MPX5100 */
#endif
      break;
    case 6: /* Overheating protection fuse on P1.4. Low if OK, high if blown. */
      val = (P1IN & BIT4) ? 1 : 0;
      break;
    case 7: /* +12V/11 + 300mV (diode drop)  */
      val = mult_u16xu16h(val*11, (VREF<<4)) + 300;
      break;
    case 8:
      /* revolutions per minute */
      val = div_u32_u16(JFREQ*60, gCafPeriod);
      break;
    case 10: /* VTEMP=0.00355(TEMPC)+0.986 example: 1312 */
      //val = (val*(VREF/4095) - 0.986) / 0.00355;
      //val = val*(VREF/4095)*(1/0.00355) - (0.986/0.00355) + 50;
      val = mult_u16xu16h(val, 14877) - (278-50);
      break;
    case 11: /* AVcc/2 example: 2147 2759 */
      val = mult_u16xu16h(val, VREF<<3);
      break;
  }
#else
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
#endif
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
#if defined(__MSP430_449__)
  ADC12CTL0 = SHT1_4|SHT0_4|REFON|REF2_5V|ADC12ON;
  ADC12CTL1 = CSTARTADD_0|SHS_0|SHP|ADC12DIV_0|ADC12SSEL_0|CONSEQ_0;
#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
  ADC12CTL0 = SHT1_6|SHT0_6|MSC|ADC12ON;
  ADC12CTL1 = CSTARTADD_0|SHS_1|SHP|ADC12DIV_0|ADC12SSEL_0|CONSEQ_3;
#else
#error Unsupported processor
#endif  
  
  ADC12IFG = 0;

#ifdef __MSP430_449__
  ADC12IE = 0;
#else
  /* Trigger interrupt for first and last ADC12MEM for data validation */
  ADC12IE = (1<<11) | (1<<0);
#endif

  /* Setup channel references and memory */
#if defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
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
  P6SEL = 0xff;
  P6DIR = 0x00;
  
  /* Setup CAF RPM measurement interrupt */
  P1DIR &= ~BIT5;
  P1SEL &= ~BIT5;
  P1IES &= ~BIT5;
  P1IE |= BIT5;
  P1IFG &= ~BIT5;

#else
  ADC12MCTL0 = INCH_0|SREF_1;
  ADC12MCTL1 = INCH_1|SREF_1;
  ADC12MCTL2 = INCH_2|SREF_1;
  ADC12MCTL3 = INCH_3|SREF_1;
  ADC12MCTL4 = INCH_10|SREF_1; /* Temp */
  ADC12MCTL5 = INCH_11|SREF_1; /* AVcc */
  P6SEL = 0xff;
  P6DIR = 0x00;
#endif

#ifndef __MSP430_449__
  ADC12CTL0 |= ENC;
#endif
}

#ifndef __MSP430_449__

#define CC_MIN (1)
static unsigned int ccSetPoint; /* Compressor set point in mBar */
#if defined(CCPID_ENABLE)
static signed int cc_x1, cc_i;
/* Note: Pressure values are already in ACTuator domain, so this factors do not
   require to be corrected by ACTMAX. */
#define CC_P (15000)
#define CC_I (15000)
#define CC_D (1000)
#endif

#define CF_MIN (140)
static unsigned int cfSetPoint; /* Combustion fan set point in revolutions per second */
#if defined(CFPID_ENABLE)
static signed int cf_x1, cf_i;
/* Note: CAF revolution per second sensor values are not in ACTuator domain,
   so the PID parameters need to be corrected by ACTMAX */
#ifdef CF_TOYBLOWER
#define CF_P (300<<ACTMAX_LD2)
#define CF_I (200<<ACTMAX_LD2)
#define CF_D (100<<ACTMAX_LD2)
#else
#define CF_P (500<<ACTMAX_LD2)
#define CF_I (200<<ACTMAX_LD2)
#define CF_D (100<<ACTMAX_LD2)
#endif
#endif


#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
/*
 * Nozzle air compressor, combustion air fan and glow plugs control
 */
static HANDLE_TIMER hTimerCtrl;
#define CTRL_ITER_PERIOD 12

static int ctrlIterate(HANDLE_TIMER hTimer, void *data)
{
  signed int val;
  signed int d, err;
#ifdef GK_MODULATE
  static unsigned int ctrl_counter;
#endif

  machine_led_set(1);

  if (ccSetPoint > CC_MIN)
  {
#ifdef CCPID_ENABLE
    /* calc error */
    err = (ccSetPoint - adc_read_single(5));
    /* calculate derivative and integral part */
    {
      signed int test = (cc_i>>1) + (err>>1);
      cc_i += err;
      if ( (cc_i ^ test) & 0x8000 ) {
        if (test > 0) {
          cc_i = 32767;
        } else {
          cc_i = -32768;
        }
      } 
    }
    d = err-cc_x1;
    /* calculate new output */
    val = mac_s16xs16_h3(err,CC_P, d,CC_D, cc_i,CC_I);

    /* update states */
    cc_x1 = err;
#else
    val = mult_u16xu16h(ccSetPoint<<2, div_u32_u16(VCCREF*65536/4, adc_read_single(7)));
#endif    
    /* clamp output */
    if (val > ACTMAX) {
      val = ACTMAX;
    }
    if (val < 0) {
      val = 0;
    }
    /* Set new output value */
    TBCCR5 = ACT2PWM(val); /* ACT_CC */
  } else {
    TBCCR5 = ACT2PWM(0);
    cc_x1 = 0;
    cc_i = 0;      
  }

  /* Handle period Timeout in the PID iteration time raster */
  if (gCafCounter > ((60*TFREQ)/(CTRL_ITER_PERIOD*CF_MIN)) ) {
    gCafPeriod = 65535;
  } else {
    gCafCounter++;
  }

  if (cfSetPoint > CF_MIN)
  {
#ifdef CFPID_ENABLE
    /* calc error */
    err = (cfSetPoint - adc_read_single(8));
    /* calculate derivative and integral part */
    {
      signed int test = (cf_i>>1) + (err>>1);
      cf_i += err;
      if ( (cf_i ^ test) & 0x8000 ) {
        if (test > 0) {
          cf_i = 32767;
        } else {
          cf_i = -32768;
        }
      } 
    }
    d = err-cf_x1;
    /* calculate new output */
    val = mac_s16xs16_h3(err,CF_P, d,CF_D, cf_i,CF_I);

    /* update states */   
    cf_x1 = err;
#else
    val = mult_u16xu16h(cfSetPoint<<2, div_u32_u16(VCCREF*65536/4, adc_read_single(7)));
#endif

    /* clamp output */
    if (val > ACTMAX) {
      val = ACTMAX;
    }
    if (val < 0) {
      val = 0;
    }
    /* Set new output value */
    TBCCR6 = ACT2PWM(val); /* ACT_CF */
  } else {
    TBCCR6 = ACT2PWM(0);
    cf_x1 = 0;
    cf_i = 0;
  }
  
#ifdef GK_MODULATE
  ctrl_counter++;
  if (ctrl_counter >= (ACTMAX>>GK_SCALE)) {
    ctrl_counter = 0;
  }

  if ( ctrl_counter < gkzSetPoint ) { /* ACT_GKZ */
    P1OUT |= BIT0;
  } else {
    P1OUT &= ~BIT0;
  }
  if ( ctrl_counter < gkphSetPoint ) { /* ACT_GKPH */
    P1OUT |= BIT1;
  } else {
    P1OUT &= ~BIT1;
  }
#endif /* GK_MODULATE */

  gADCstatus |= 1;

  machine_led_set(0);
  
  return 1;
}
#endif /* CCPID_ENABLE || CFPID_ENABLE */

/*
 * Fuel pump (dosing pump) timer for pulse generation
 */
static HANDLE_TIMER hTimerFp;
static int timerFpReload;

#define FP_PULSE 4
#define FP_PREHEATING

static
int timerFpCb(HANDLE_TIMER hTimer, void *data)
{
  if ((P4OUT & BIT1) == 0) {
    machine_timer_reset(hTimerFp, FP_PULSE);
    P4OUT |= BIT1;
  } else {
    P4OUT &= ~BIT1;
    machine_timer_reset(hTimerFp, timerFpReload);
  }
  /* stay awake */
  return 1;
}

#define FP_PERIOD(x) (((128*20)/(x))-FP_PULSE)

/* Timer period values for dosing fuel pump */
static const unsigned int dp_periods[0x40] = 
{
  FP_PERIOD(1),  FP_PERIOD(1),  FP_PERIOD(2),  FP_PERIOD(3),  FP_PERIOD(4),  FP_PERIOD(5),  FP_PERIOD(6),  FP_PERIOD(7),
  FP_PERIOD(8),  FP_PERIOD(9),  FP_PERIOD(10), FP_PERIOD(11), FP_PERIOD(12), FP_PERIOD(13), FP_PERIOD(14), FP_PERIOD(15),
  FP_PERIOD(16), FP_PERIOD(17), FP_PERIOD(18), FP_PERIOD(19), FP_PERIOD(20), FP_PERIOD(21), FP_PERIOD(22), FP_PERIOD(23),
  FP_PERIOD(24), FP_PERIOD(25), FP_PERIOD(26), FP_PERIOD(27), FP_PERIOD(28), FP_PERIOD(29), FP_PERIOD(30), FP_PERIOD(31), 
  FP_PERIOD(32), FP_PERIOD(33), FP_PERIOD(34), FP_PERIOD(35), FP_PERIOD(36), FP_PERIOD(37), FP_PERIOD(38), FP_PERIOD(39),
  FP_PERIOD(40), FP_PERIOD(41), FP_PERIOD(42), FP_PERIOD(43), FP_PERIOD(44), FP_PERIOD(45), FP_PERIOD(46), FP_PERIOD(47), 
  FP_PERIOD(48), FP_PERIOD(49), FP_PERIOD(50), FP_PERIOD(51), FP_PERIOD(52), FP_PERIOD(53), FP_PERIOD(54), FP_PERIOD(55),
  FP_PERIOD(56), FP_PERIOD(57), FP_PERIOD(58), FP_PERIOD(59), FP_PERIOD(60), FP_PERIOD(61), FP_PERIOD(62), FP_PERIOD(63)
};

static
int get_dp_value(int dp)
{
  int shift=0;
  
  if (dp & 0x40) {
    shift++;
  }
  if (dp & 0x80) {
    shift+=2;
  }
  return dp_periods[dp>>shift]>>shift;
}

void machine_act(unsigned short act[], int n)
{
  if (act[4]) { /* ACT_VF */
    P4OUT |= BIT4;
  } else {
    P4OUT &= ~BIT4;
  }

  if (act[2]) { /* ACT_CP */
    P4OUT |= BIT2;
  } else {
    P4OUT &= ~BIT2;
  }

#ifdef GK_MODULATE
  gkzSetPoint = act[7]>>GK_SCALE;  /* ACT_GKZ */
  gkphSetPoint = act[8]>>GK_SCALE; /* ACT_GKPH */
#else
  if ( act[7] ) { /* ACT_GKZ */
    P1OUT |= BIT0;
  } else {
    P1OUT &= ~BIT0;
  }
  if ( act[8] ) { /* ACT_GKPH */
    P1OUT |= BIT1;
  } else {  
    P1OUT &= ~BIT1;
  }
#endif

  ccSetPoint = act[5]; /* ACT_CC */
  cfSetPoint = act[6]; /* ACT_CF */

  if (act[1] != 0) { /* ACT_DP */
#ifdef FP_PREHEATING
    if (act[1] == 1) {
      /* Keep dosing pump energized in case of special value "1". 
         The purpose is to preheat the pump to avoid "cold thick oil" problems.
         Do not leave the pump too long in that state ! YMMV */
      P4OUT |= BIT1;
    } else
#endif /* FP_PREHEATING */
    {
      timerFpReload = get_dp_value(act[1]);
      if (hTimerFp == NULL) {
        hTimerFp = machine_timer_create(timerFpReload, timerFpCb, NULL);
      }
    }
  } else {
    machine_timer_destroy(hTimerFp);
    hTimerFp = NULL;
    P4OUT &= ~BIT1;
  }
}


static int pack=0;

void machine_ack(int ack)
{
  if (ack == pack) {
    return;
  }

  if (ack) {
    /* activate TimerB */
#ifdef USE_RPWM
    srand(TBR);
#endif
    TBCTL = 0;
    TBCCR0 = ACTMAX-1;    /* ~8MHz / (pwmmax-1) = xHz PWM frequency */
    TBCCR1 = ACT2PWM(0);
    TBCCR2 = ACT2PWM(0);
    TBCCR3 = ACT2PWM(0);
    TBCCR4 = ACT2PWM(0);
    TBCCR5 = ACT2PWM(0);
    TBCCR6 = ACT2PWM(0);

#define TBCCTL_INIT CLLD_0|OUTMOD_7

    TBCCTL0 = TBCCTL_INIT;
    TBCCTL1 = TBCCTL_INIT;
    TBCCTL2 = TBCCTL_INIT;
    TBCCTL3 = TBCCTL_INIT;
    TBCCTL4 = TBCCTL_INIT;
    TBCCTL5 = TBCCTL_INIT;
    TBCCTL6 = TBCCTL_INIT;
#ifdef USE_RPWM
    /* Count upto TBCCR0, clk is SMCLK, input divider 2 */
    TBCTL = TBCLGRP_0|CNTL_0|TBSSEL_2|ID_1|MC_1|TBCLR;
#else
    /* Count upto TBCCR0, clk is SMCLK, input divider 1 */
    TBCTL = TBCLGRP_0|CNTL_0|TBSSEL_2|ID_0|MC_1|TBCLR;
#endif
    TBCTL |= TBIE;
    P4SEL = BIT6|BIT5;

    /*  Control iteration timer */
#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
#ifdef CCPID_ENABLE
    cc_i = 0;
    cc_x1 = 0;
#endif
#ifdef CFPID_ENABLE
    cf_i = 0;
    cf_x1 = 0;
#endif
    if (hTimerCtrl == NULL) {
      gCafCounter = 100;
      hTimerCtrl = machine_timer_create(CTRL_ITER_PERIOD, ctrlIterate, NULL);
    } 
#endif

    /* Increase ADC12 sampling frequency */
    adc12period = ADCTRIG_FAST;
    TACCR1 = TACCR0 + adc12period;

    /* activate subsystem power */
    P3OUT |= BIT3;

  } else {

#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
    machine_timer_destroy(hTimerCtrl);
    hTimerCtrl = NULL;
#endif

    /* shutdown subsystem power */
    P3OUT &= ~BIT3;

    /* deactivate TimerB, outputs and control loops. Bit 5,6 are active low PWM outs, CF and CC */
    P4OUT = BIT5|BIT6;

    P4SEL = 0;
    TBCCTL0 = CLLD0|OUTMOD_0;
    TBCCTL1 = CLLD0|OUTMOD_0;
    TBCCTL2 = CLLD0|OUTMOD_0;
    TBCCTL3 = CLLD0|OUTMOD_0;
    TBCCTL4 = CLLD0|OUTMOD_0;
    TBCCTL5 = CLLD0|OUTMOD_0;
    TBCCTL6 = CLLD0|OUTMOD_0;                        
    TBCTL = 0;
    P1OUT &= ~(BIT0|BIT1);
    adc12period = ADCTRIG_SLOW;
  }
  pack = ack;
}

#endif /* ! __MSP430_449__ */

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

  /* Clear LCD Display */
  for (i=0; i<20; i++) {
    LCDMEM[i]=0;
  }

  LCDCTL = LCDON + LCD4MUX + LCDP0 + LCDP1 + LCDP2;       /* LCD 4Mux, S0-S39 */
  BTCTL = BTSSEL | BT_fCLK2_DIV2 | BT_fLCD_DIV64 | BTDIV; /* LCD freq ACLK/64 */
                                                          /* IRQ freq ACLK/256/2 */
  P5SEL = 0xFC;

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
 	
  for (i=0; i<MAX_FASTTIMERS; i++)
  {
    if (ftimers[i].enabled)
    {
      ftimers[i].ticks--;
      if (ftimers[i].ticks==0)
      {
        ftimers[i].ticks = ftimers[i].reload;
        wup |= ftimers[i].func((HANDLE_TIMER)&ftimers[i], ftimers[i].data);
      }
    }
  }

  if (wup)
    machine_wakeup();
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

#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)

static void msp430_core_init(void)
{
  /* MCLK = DCO @ ~8MHz, SMCLK = MCLK */
  DCOCTL = DCO2|DCO1|DCO0;
  BCSCTL1 = XT2OFF|DIVA_0|RSEL2|RSEL1|RSEL0;
  BCSCTL2 = SELM_0|DIVM_0|DIVS_0;
}

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

  /* ADC12 sensor inputs */
  P6SEL = BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6|BIT7;
  P6DIR = 0;
  P6OUT = 0;

  ADC12CTL0 = 0;

  /* Timer A as main timer source and ADC12 trigger */
  TACTL = 0;
  TACCTL0 = OUTMOD_0|CCIE;
  TACCTL1 = OUTMOD_4|CCIE;
  TACCTL2 = 0;
  /* Timer A clock 32768Hz / 8 = 4096Hz (JFREQ) */
  TACCR0 = JFREQ; /* RTC 1Hz time base */
  adc12period = ADCTRIG_SLOW; /* ADC12 trigger clock 0.1Hz (10Hz while active) */
  TACCR1 = adc12period;
  TACTL = TASSEL_1|ID_3|MC_2|TACLR;

  /* disable any peripheral which is not always on */
  TBCTL = 0;
  U0CTL = 0;
  U1CTL = 0;  

}

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
  /* 128 Hz interval */
  switch (TAIV) {
#if (__MSPGCC__ >= 20110706)
    case TAIV_TACCR1:
#else
    case TAIV_CCR1:
#endif
      TACCR1 = TACCR1 + adc12period;   /* 10 or 0.1 Hz trigger for ADC12 */
      break;
  }
}

#endif

void machine_init(void)
{
  int i;
  
  WDTCTL = WDTPW | WDTHOLD;     /* stop watchdog timer */
  
  /* Setup RTC */
  rtc_init();

  machine_init_io();

  /* Setup clocks */
  msp430_core_init();

  /* Setup adc for sensor reading */
  adc_init();

#ifndef __MSP430_449__
  adc_invalidate();
  pack=0;
  hTimerFp=NULL;
#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
  hTimerCtrl=NULL;
#endif
#endif /* !__MSP430_449__  */

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

#if  defined(__MSP430_449__)

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

static int backlight_was_on=0;

void machine_lcd(int enable)
{
  /* Enable or disable the LCD display only once to avoid the backlight from toggling. */
  if (enable) {
    if ( (LCDCTL & LCDON) == 0 ) {
      LCDCTL |= LCDON;
      machine_backlight_set(backlight_was_on);
    }
  } else {
    if ( LCDCTL & LCDON ) {
      LCDCTL &= ~LCDON;
      backlight_was_on = machine_backlight_get();
      machine_backlight_set(0);
    }
  }
}
#else
void machine_beep(void)
{
}

int machine_backlight_get(void)
{
  return 0;
}

void machine_backlight_set(int en)
{
}

unsigned int machine_buttons(int do_read)
{
#ifdef POELI_UART1_ON
  /* Treat UART 1 line as "button", active low */
  return (P3IN & BIT7) ? 0 : 1;
#else
  return 0;
#endif
}

void machine_lcd(int enable)
{

}
#endif

void machine_led_set(int s)
{
#if  defined(__MSP430_449__)
  if (s)
    P1OUT &= ~BIT3;
  else
    P1OUT |= BIT3;
#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
  if (s)
    P1OUT |= BIT3;
  else
    P1OUT &= ~BIT3;
#endif
}

unsigned int machine_getJiffies(void)
{
#ifdef __MSP430_449__
  return (BTCNT2<<8)|BTCNT1;
#else
  return TAR;
#endif
}

#ifdef __MSP430_449__
interrupt(PORT1_VECTOR) msp430_port1_interrupt(void)
{
  P1IFG = 0;
  machine_wakeup();
}

int machine_stayAwake(void)
{
  int result;
  
  result = ((P3IN & 0xf0) != 0xf0) || (P1IN & BIT7);
  
  return result;
}
#else
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
  unsigned short p_block_write[sizeof(FCN_SIZE)/sizeof(short)];
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

  /* Write new data */  
  for (; words>0; words-=32) {
    if ( ((size_t)fptr & 0x1ff) == 0) { 
      /* Erase segment */
      FCTL3 = FWKEY;          /* Unlock */
      FCTL1 = FWKEY | ERASE;  /* Erase segment */
      *fptr = 0xff;
    }
    pf_block_write(fptr, rptr, (words > 32) ? 32 : words);
    fptr += 64;
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
  int words = (nbytes+1)>>1;

  machine_led_set(1);

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
    
  /* Write new data */
  for (; words>0; words--) {
    FCTL3 = FWKEY;          /* Unlock */
    FCTL1 = FWKEY | WRT;
    if (rptr != NULL) {
      *fptr++ = *rptr++;
    } else {
      *fptr++ = 0;
    }
    FCTL3 = FWKEY|LOCK;     /* Lock */
    FCTL1 = FWKEY;
  }

  /* Restore Watchdog settings */
  WDTCTL = wdg | WDTPW;
  /* Enable interrupts */
  eint();
  
  machine_led_set(0);
}
#endif
