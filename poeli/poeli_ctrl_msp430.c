/*
 * Actuator and sensor control
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */

#include "poeli_ctrl.h"
#include "machine.h"

#if defined(__MSP430_169__) || defined(__MSP430_1611__)
#include <msp430x16x.h>
#elif defined(__MSP430_149__)
#include <msp430x14x.h>
#else
#error unsupported
#endif

#include <legacymsp430.h>
#include <stdio.h>
#include <string.h>


#ifdef POELI_HW

/* #define USE_RPWM */

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

  switch (iv) {
  #if 0
    case 4:  if (p2& DIAG_CP) gP2StateOff |= DIAG_CP; else gP2StateOff &= ~DIAG_CP; break; /* CP */
  #endif
    case 12: if (p2& DIAG_CC) gP2StateOff |= DIAG_CC; else gP2StateOff &= ~DIAG_CC; break; /* CC */
    case 14: if (p2& DIAG_CF) gP2StateOff |= DIAG_CF; else gP2StateOff &= ~DIAG_CF; break; /* CF */
  }
}

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

#define VREF 3300
#define VCCREF 12500

int poeli_ctrl_read_single(int c)
{
  unsigned int val;

#ifndef POELI_HW
  ADC12CTL1 = (ADC12CTL1 & 0x0fff) | (c<<12);
  ADC12CTL0 |= ADC12SC|ENC;
  while (ADC12CTL1 & ADC12BUSY) ;
  val = ADC12MEM[c];
  ADC12CTL0 &= ~ENC;
#else
  val = ADC12MEM[c];
#endif

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

  return val;
}

void poeli_ctrl_read(unsigned short s[], int n)
{
  int i;

  for (i=0; i<n; i++) {
    s[i] = poeli_ctrl_read_single(i);
  }
}

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

#ifdef GK_MODULATE
static unsigned int gkphSetPoint;
static unsigned int gkzSetPoint;
#define GK_SCALE 5
#endif

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
    err = (ccSetPoint - poeli_ctrl_read_single(5));
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
    val = mult_u16xu16h(ccSetPoint<<2, div_u32_u16(VCCREF*65536/4, poeli_ctrl_read_single(7)));
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
    err = (cfSetPoint - poeli_ctrl_read_single(8));
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
    val = mult_u16xu16h(cfSetPoint<<2, div_u32_u16(VCCREF*65536/4, poeli_ctrl_read_single(7)));
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
    adc_interval_set(1);

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
    adc_interval_set(0);
  }
  pack = ack;
}

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
  P6SEL = 0xff;
  P6DIR = 0x00;
  
  /* Setup CAF RPM measurement interrupt */
  P1DIR &= ~BIT5;
  P1SEL &= ~BIT5;
  P1IES &= ~BIT5;
  P1IE |= BIT5;
  P1IFG &= ~BIT5;

  ADC12CTL0 |= ENC;
}

void poeli_ctrl_init(void)
{
  adc_invalidate();
  pack=0;
  hTimerFp=NULL;
#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
  hTimerCtrl=NULL;
#endif
}

#endif /* POELI_HW */
