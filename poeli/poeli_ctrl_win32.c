/*
 * Actuator and sensor control
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */

#include "poeli_ctrl.h"
#include "machine.h"

#include <stdio.h>
#include <string.h>


#ifdef POELI_HW

#define CCPID_ENABLE
#define CFPID_ENABLE

/* Modulate GK and Nozzle stock preheating power */
//#define GK_MODULATE

#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/time.h>

static int shmid;
static unsigned short (*d)[16];

/* Actuator ouput pin state while driver on or off. */
unsigned char gP2StateOff, gP2StateOn;

static unsigned char gADCstatus=0;

void poeli_ctrl_adc12_interrupt(void)
{
  unsigned int ifg;

  ifg = 0;

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
}

static unsigned int gCafPeriod = 0;
static unsigned int gCafCounter = 0;

int poeli_ctrl_read_single(int c)
{
  return d[1][c];
}
 
void poeli_ctrl_read(unsigned short *s, int n)
{
  int i;
  /* Write back virtual sensors */
  d[1][10] = s[10];
  d[1][9] = s[9];  

  for (i=0; i<n; i++) {
    s[i] = d[1][i];
  }
}  
   
void poeli_ctrl_invalidate(void)
{
  d[2][0] = 0;
}

int poeli_ctrl_is_uptodate(void)
{
  /* return true if actuator were updated and ADCMEM0 through ADCMEM11 are up to date */
  return ( gADCstatus == 7 );
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

/* Timer A OUT1 toggles ADC 12 trigger signal, thus a factor 2 in the periods. */
#define ADCTRIG_SLOW (JFREQ*5)
#define ADCTRIG_FAST (JFREQ/20)
static unsigned int adc12period;

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
    /* ACT_CC = val*/
  } else {
    /* ACT_CC = 0 */
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
    /* ACT_CF = val */
  } else {
    /* ACT_CF = 0 */
    cf_x1 = 0;
    cf_i = 0;
  }
  
#ifdef GK_MODULATE
  ctrl_counter++;
  if (ctrl_counter >= (ACTMAX>>GK_SCALE)) {
    ctrl_counter = 0;
  }

  if ( ctrl_counter < gkzSetPoint ) { /* ACT_GKZ */
    /* GKZ on */
  } else {
    /* GKZ off */
  }
  if ( ctrl_counter < gkphSetPoint ) { /* ACT_GKPH */
    /* GKPH on */
  } else {
    /* GKPH off */
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

static int fp; /* dosing fuel pump state */

static
int timerFpCb(HANDLE_TIMER hTimer, void *data)
{
  if (fp == 0) {
    machine_timer_reset(hTimerFp, FP_PULSE);
    fp = 1;
  } else {
    fp = 0;
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

void poeli_ctrl_act(unsigned short act[], int n)
{
  if (act[4]) { /* ACT_VF */
    /* VF on */
  } else {
    /* VF off */
  }

  if (act[2]) { /* ACT_CP */
    /* CP on */
  } else {
    /* CP off */
  }

#ifdef GK_MODULATE
  gkzSetPoint = act[7]>>GK_SCALE;  /* ACT_GKZ */
  gkphSetPoint = act[8]>>GK_SCALE; /* ACT_GKPH */
#else
  if ( act[7] ) { /* ACT_GKZ */
    /* GKZ on */
  } else {
    /* GKZ off */
  }
  if ( act[8] ) { /* ACT_GKPH */
    /* GKPH on */
  } else {  
    /* GKPH off */
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
      fp = 1;
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
    fp = 0;
  }
}


static int pack=0;

void poeli_ctrl_ack(int ack)
{
  if (ack == pack) {
    return;
  }

  if (ack) {
    /* activate TimerB */
#ifdef USE_RPWM
    srand(TBR);
#endif

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

    /* activate subsystem power */

  } else {

#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
    machine_timer_destroy(hTimerCtrl);
    hTimerCtrl = NULL;
#endif

    /* shutdown subsystem power */

    /* deactivate TimerB, outputs and control loops. Bit 5,6 are active low PWM outs, CF and CC */
    adc12period = ADCTRIG_SLOW;
  }
  pack = ack;
}

void poeli_ctrl_init(void)
{
  shmid = shmget(0xb0E1, 4096, 0666 | IPC_CREAT);
  if (shmid == -1) {
    printf("shmget() failed\n");
  }
  d = shmat(shmid, (void *)0, 0);
  if ((int)d == -1) {
    printf("shmat() failed (d)\n");
  }

  adc_invalidate();
  pack=0;
  hTimerFp=NULL;
#if defined(CCPID_ENABLE) || defined(CFPID_ENABLE)
  hTimerCtrl=NULL;
#endif
}

#endif /* POELI_HW */
