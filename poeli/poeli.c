/*
 * POELI. Parking Heater controller.
 */

#include "kernel.h"
#include "machine.h"
#include "poeli_ctrl.h"
#include "wbus.h"
#include "wbus_server.h"
#include <stdio.h>
#include <string.h>

/* Static data */
static heater_state_t heater_state;
static HANDLE_WBUS w;
static unsigned char gfActive;          /* Flag indicating W-Bus active mode, fast sensor monitoring/update. */
static unsigned char gSensorsUpdated;   /* Flag indicating up to date sensor data.  */
static unsigned int gActiveTimout;      /* Last W-Bus received time stamp. */

#ifdef __linux__
#include <stdlib.h>
#endif

/* High side switch diagnose pins on P2 */
enum {
  S_VF = 0,
  S_CP,
  S_AUX,
  S_DP,
  S_NONE,
  S_CC
};

static
unsigned short poeli_calc_fd(heater_state_t *hs)
{
  static unsigned short x1=0, x=0;
  static const int K1=5, K2=350, thm = 3200, thl = 3000, thh = 3400;
  static short dx, th, m;

  /* Averaging filter. */
  x = (x*3 + hs->volatile_data.sensor[SENSOR_GPR])>>2;
  /* derivative. */
  dx=(x-x1);

  /* Adaptive observer threshold selection. */
  th = thm;
  if (dx < -10) {
    th = thl;
  }
  if (dx > 10) {
    th = thh;
  }

  /* Calculate observer value. */
  m =  x*K1 + dx*K2;

  /* Store previous sensor value for derivative. */
  x1=x;

  /* Apply adaptive threshold to observer value. */
  return (m > th) ? 1 : 0;
}

static
unsigned short poeli_calc_he(heater_state_t *hs)
{
  int nrg = 0;
  /* 
   Heating power in Watt P = [Wh/L] * 3600[s] * [L/s] * [1/s] * efficiency
   Energy density assumed: 10000 [Wh/L]
   Dosing pump liter per puls:  0.000035 [L/s]
   Dosing pump frequency: hs->volatile_data.act[ACT_DP] 1/20 [Hz]
   Estimated efficiency: 70 %
   const int factor = (int)(10000.0 * 3600.0 * 0.000035 * (1.0/20.0) * 0.7);
   */
  nrg = hs->volatile_data.act[ACT_DP] * 44;
  
  return nrg;
}

TASK_FUNC(poeli_read_sensors)
{
  while (1) {
    unsigned int sleept;
    int maybeSensorsUpdated;
    
    maybeSensorsUpdated = adc_is_uptodate();

    poeli_ctrl_read(heater_state.volatile_data.sensor, 9);

    sleept = MSEC2JIFFIES(100);

    switch (heater_state.volatile_data.status) {
      case HT_OFF:
      case HT_LOCKED:
        sleept += MSEC2JIFFIES(9900);
      case HT_START:
      case HT_GLOW:
      case HT_TEST:
      case HT_PREHEAT:
      case HT_VENT:
      case HT_END:
        heater_state.volatile_data.sensor[SENSOR_FD] = 0;
        heater_state.volatile_data.sensor[SENSOR_HE] = 0;
        break;
      default:
        /* calculate virtual sensors */
        heater_state.volatile_data.sensor[SENSOR_FD] = poeli_calc_fd(&heater_state);
        heater_state.volatile_data.sensor[SENSOR_HE] = poeli_calc_he(&heater_state);
        break;
    }

    if (maybeSensorsUpdated && gSensorsUpdated == 0) {
      gSensorsUpdated = 1;
    }

    /* Increase sensor update frequency in case of W-Bus activity. */
    if ( gfActive && ((machine_getJiffies() - gActiveTimout) > MSEC2JIFFIES(10000)) ) {
      gfActive = 0;
    }
    if ( gfActive ) {
      sleept = MSEC2JIFFIES(100);
    }

    kernel_sleep(sleept);
    kernel_yield();
  }
}

/**
 * Do heater transition to a new status.
 * \return state changed
 */
static
int poeli_heater_switch_status(heater_state_t *h, heater_status_t s)
{
  const heater_seqmem_t *seq;
  heater_status_t ps;
  int i;

  /* previous state */
  ps = h->volatile_data.status;

  /* Assimilate a new status */
  if (h->volatile_data.status_sched != HT_NONE) { 
    s = h->volatile_data.status_sched;
    h->volatile_data.status_sched = HT_NONE;
  }
  h->volatile_data.status = s;
  seq = &seq_data.heater_seq[s];

  switch (s) {
    case HT_VENT:
    case HT_BURN_L:
    case HT_BURN_H:
      if (h->volatile_data.wbus_time > h->volatile_data.time) {
        h->volatile_data.wbus_time -= h->volatile_data.time;
      }
      h->volatile_data.time = h->volatile_data.wbus_time;
      break;
    case HT_TEST:
      h->volatile_data.time = h->volatile_data.wbus_time;
      break;
    case HT_RAMPUP:
    case HT_RAMPDOWN:
      if (h->volatile_data.wbus_time > seq->seq.time) {
        h->volatile_data.wbus_time -= seq->seq.time;
      }
    default:
      h->volatile_data.time = seq->seq.time;
      break;
  }

  /* Clear short term fault list */
  for (i=0; i<HT_LAST; i++) {
    h->volatile_data.faults[i]=0;
  }
  
  return (ps != s); 
}

#include "../wbus/wbus_const.h"
static const unsigned char sensor_code[NUM_SENSOR] = 
{ ERR_INTGP, ERR_INTT, ERR_INTNSH, ERR_INTFPW, ERR_INTGP2, ERR_P, ERR_OH, ERR_VCCLOW, ERR_INTCAF, ERR_UNKNOWN, ERR_SCGP };

static
void poeli_heater_iterate(heater_state_t *h)
{
  const heater_seqmem_t *seq;
  static unsigned char tsec = 0;
  int i, stateChanged;

  stateChanged = 0;

  /* Current sequence */
  seq = &seq_data.heater_seq[h->volatile_data.status];

  /* check time */
  if (h->volatile_data.time > 0) {
    h->volatile_data.time --;
  } else {
    stateChanged = poeli_heater_switch_status(h, seq->seq.status_next);
    seq = &seq_data.heater_seq[h->volatile_data.status];
  }

  /* check sensors */
  if (gSensorsUpdated && !stateChanged)
  {
    unsigned short *s = h->volatile_data.sensor;
    unsigned char *f = h->volatile_data.faults;

    for (i=0; i<NUM_SENSOR; i++) {
      if (s[i] < seq->seq.sensor_min[i] || s[i] > seq->seq.sensor_max[i]) {
        PRINTF("State %d Sensor %d failure, current = %d, min = %d, max = %d\n", h->volatile_data.status, i, s[i], seq->seq.sensor_min[i], seq->seq.sensor_max[i]);
        /* increment fault counter */
        f[i]++;

        /* if too many faults, then take "if failure" state to continue */
        if ( f[i] > seq->seq.max_faults ) {
          heater_status_t next_status;
          if (s[i] < seq->seq.sensor_min[i]) {
            next_status = seq->seq.on_sensor_min[i];
          } else {
            next_status = seq->seq.on_sensor_max[i];
          }

          /* register error if next state indicates a serious error. */
          if ( ! (seq->seq.fault_mask & (1<<i)) )
          {
            /* Turn off everything as a first thing. */
            machine_ack(0);
            /* Record the error */
            wbus_error_add(h, sensor_code[i], i, wbdata+128);
            PRINTF("fault mask %d\n", seq->seq.fault_mask);
          }

          stateChanged = poeli_heater_switch_status(h, next_status);
          seq = &seq_data.heater_seq[h->volatile_data.status];
#ifdef __linux__
          for (i=0; i<NUM_ACT; i++) {
            PRINTF("act[%d] = %d\n", i, h->volatile_data.act[i]);
          }
          for (i=0; i<NUM_SENSOR; i++) {
            PRINTF("sensor[%d] = %d\n", i, s[i]);
          }
#endif
          break;
        }
      } else {
        /* if sensor value is in the desired range, decrement fault counter */
        if ( f[i] > 0 ) {
          f[i]--;
        }
      }
    }
  }

  /* Command refresh handling */
  if ( h->volatile_data.status != HT_OFF 
    && h->volatile_data.status != HT_TEST
    && h->volatile_data.status != HT_LOCKED 
    && h->volatile_data.status != HT_STOP
    && h->volatile_data.status != HT_COOLDOWN
    && h->volatile_data.status != HT_END  )
  {
    if (h->volatile_data.cmd_refresh_time > 0) {
      h->volatile_data.cmd_refresh_time--;
    } else {
      heater_status_t ns = HT_OFF;
      
      PRINTF("Command refresh timed out\n");
      if ( h->volatile_data.status >= HT_IGNITE
        && h->volatile_data.status < HT_STOP )
      {
        ns = HT_STOP;
      }
      wbus_error_add(h, ERR_REFRESH, 0, wbdata+128);
      stateChanged = poeli_heater_switch_status(h, ns);
      seq = &seq_data.heater_seq[h->volatile_data.status];
      h->volatile_data.cmd_refresh = 0;
    }
  } 
  if (h->volatile_data.status == HT_OFF && h->volatile_data.cmd_refresh ) {
    h->volatile_data.cmd_refresh = 0;
  }

  /* Handle actuators */
  if (h->volatile_data.status != HT_TEST)
  {
    unsigned short *a = h->volatile_data.act;
    for (i=0; i<NUM_ACT; i++) {
      signed long step;
      
      step = (signed long)seq->seq.act_step[i]*(long)h->volatile_data.time;
      if (i==ACT_DP) {
        a[i] = seq->seq.act_target[i] - (step>>STEP_SCALE);
      } else {
        a[i] = (unsigned short)((((signed long)seq->seq.act_target[i]<<STEP_SCALE) - step)>>(STEP_SCALE-ACTMAX_LD2));
      }
    }
    /* Add CO2 calibration factor to combustion air fan value */
    a[ACT_CF] = (unsigned short) (((unsigned long)h->static_data.co2_cal * (unsigned long)a[ACT_CF])>>7);
  }
  /* Update actuators, changes to sensors might take a while. */
  machine_act(h->volatile_data.act, NUM_ACT);


  /* if the state changed, after updated hw ACTs, invalidate ADC values. */
  if (stateChanged) {
    gSensorsUpdated = 0;
    adc_invalidate();
  }

  /* Do duration accounting */
  tsec++;
  if (tsec >= (JFREQ/HEATER_PERIOD)) {
    rtc_add(&heater_state.static_data.operating_duration, 1);
    if (heater_state.volatile_data.status != HT_OFF
     || heater_state.volatile_data.status != HT_LOCKED)
    {
      rtc_add(&heater_state.static_data.working_duration, 1);
    }
    tsec = 0;
  }
}

#ifdef POELI_UART1_ON
static int prev_button = 0;
#endif

TASK_FUNC(poeli_heater_ctrl)
{
  heater_state.volatile_data.status = HT_OFF;
  heater_state.volatile_data.status_sched = HT_NONE;
  
  while (1)
  {
    poeli_heater_iterate(&heater_state);

#ifdef POELI_UART1_ON
    gfActive = 1;
#endif

    if (heater_state.volatile_data.status == HT_OFF && !gfActive) {
      machine_ack(0);
      kernel_suspend();
      kernel_yield();
    } else {
      machine_ack(1);
      kernel_sleep(HEATER_PERIOD);
      kernel_yield();
    }

#ifdef POELI_UART1_ON
    if (machine_buttons(1)) {
      heater_state.volatile_data.cmd_refresh_time = 1000;
      if (!prev_button) {
        heater_state.volatile_data.wbus_time = 60 * (60*JFREQ/HEATER_PERIOD);
        heater_state.volatile_data.status_sched = HT_START;
        heater_state.volatile_data.time = 0;
      }
      prev_button = 1;
    }
    if (!machine_buttons(1) && prev_button) {
      if ( heater_state.volatile_data.status != HT_LOCKED
        && heater_state.volatile_data.status != HT_STOP
        && heater_state.volatile_data.status != HT_COOLDOWN
        && heater_state.volatile_data.status != HT_END )
      {
        heater_state.volatile_data.status_sched = HT_STOP;
        heater_state.volatile_data.time = 0;
        heater_state.volatile_data.cmd_refresh_time = 0;
      }
      prev_button = 0;
    }
#endif
  }
}

TASK_FUNC(poeli_listen_wbus)
{
  unsigned char cmd = 0, addr;
  int err, len;

  err = wbus_open(&w, 0);
#ifdef __linux__  
  if (err) {
    exit(-1);
  }
#endif

  while (1) {
    /* Listen to W-Bus message. Blocking I/O */
    err = wbus_host_listen(w, &addr, &cmd, wbdata, &len);

    /* Assemble W-Bus response using current state info. */
    if (err ==  0) {
      err = wbus_server_process(cmd, wbdata, &len, &heater_state);
      gfActive = 1;
      gActiveTimout = machine_getJiffies();
    }

    if (heater_state.volatile_data.status_sched != HT_NONE) {
      kernel_wakeup(-1);
    }

    /* Sent assembled W-Bus message back to client. */
    err = wbus_host_answer(w, addr, cmd, wbdata, len);
    if (err) {
      PRINTF("wbus_host_answer() failed\n");
    }

    kernel_yield();
  }
}

void main(void)
{
  gActiveTimout=0;
  gfActive=1;
  gSensorsUpdated=0;
  kernel_init();
  poeli_ctrl_init();

  wbus_server_init(&heater_state);

  PRINTF("size of seq_data.heater_seq = %d\n", sizeof(seq_data));

  kernel_task_register(poeli_read_sensors, KERNEL_STACK_SIZE/3);
  kernel_task_register(poeli_heater_ctrl, KERNEL_STACK_SIZE/3);
  kernel_task_register(poeli_listen_wbus, KERNEL_STACK_SIZE/3);

  kernel_run();
}
