/*
 * WBUS server data types and function prototypes
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#ifndef __WBUS_SERVER_H__
#define __WBUS_SERVER_H__

#include "wbus.h"
#include "machine.h"

/* Heater iteration duration in JFREQ units */
#define HEATER_PERIOD (JFREQ/16)
/* Scale shift factor of actuator step values */
#define STEP_SCALE 8

enum {
  ACT_AUX = 0, /* P4 */
  ACT_DP,      /* dosing pump in Hz*10 */
  ACT_CP,      /* coolant circulation pump in percent */
  ACT_NONE,    /* guess what */
  ACT_VF,      /* ventilation fan in percent */
  ACT_CC,      /* combustion nozzle compressor in mBar */
  ACT_CF,      /* combustion fan in percent */
  ACT_GKZ,     /* ignition glow plug P1 in percent */
  ACT_GKPH,    /* preheating glow plug in percent */
  NUM_ACT
};

/* ADC12 channels on P6 */
enum {
  SENSOR_GPR = 0,   /* Glow plug, should be resistance, but its Seebeck voltage in mV not mili ohm */
  SENSOR_T0,        /* Temperature. T °C = val-50 */
  SENSOR_T1,        /* Temperature. T °C = val-50 */
  SENSOR_GKPH,      /* GKPH current in mA */
  SENSOR_GKZ,       /* PKZ  current in mA */
  SENSOR_P,         /* compressor air pressure for syphon nozzle in mBar */
  SENSOR_OVERHEAT,  /* Overheating fuse (bool) */
  SENSOR_VCC,       /* Power supply voltage in mV */
  SENSOR_CAF,       /* Combustion air fan  RPM */

  SENSOR_HE,        /* Virtual sensor: Heating power in Watt */
  SENSOR_FD,        /* Virtual sensor: Flame detector flag (bool) */
  NUM_SENSOR
};

typedef enum {
  HT_NONE = -1,
  HT_OFF       = 0,
  HT_START     = 1,
  HT_PREHEAT   = 2,
  HT_GLOW      = 3,
  HT_IGNITE    = 4,
  HT_STABILIZE = 5,
  HT_RAMPUP    = 6,
  HT_BURN_H    = 7,
  HT_RAMPDOWN  = 8,
  HT_BURN_L    = 9,
  HT_STOP      = 10,
  HT_COOLDOWN  = 11,
  HT_END       = 12,
  HT_VENT      = 13,
  HT_TEST      = 14,
  HT_LOCKED    = 15,
  HT_LAST
} heater_status_t;

typedef struct {
   unsigned long time;                        /* time in heater iteration period counts for this state in the sequence   */
   unsigned short act_target[NUM_ACT];        /* target condition for actuator after time elapsed  */
   signed short act_step[NUM_ACT];            /* actuator increment scaled by STEP_SCALE bits    */
   unsigned short sensor_min[NUM_SENSOR];     /* lower trip values for sensors                   */
   unsigned short sensor_max[NUM_SENSOR];     /* upper trip values for sensors                   */
   unsigned char on_sensor_min[NUM_SENSOR];   /* next state for each tripped lower sensor trip point */
   unsigned char on_sensor_max[NUM_SENSOR];   /* next state for each tripped upper sensor trip point */
   unsigned char max_faults;                  /* iteration counts required to acknowledge trip sensor */
   unsigned char status_next;                 /* Next state after time seconds                   */
   unsigned short fault_mask;
} heater_seq_t;

/* heater program sequence element */
typedef union {
  heater_seq_t seq;
  unsigned char fill[128];
} heater_seqmem_t;

typedef struct {
  /* Data that should be persistent some how (flash backup) */
  struct {
    rtc_time_t working_duration;
    rtc_time_t operating_duration;
    unsigned long counter;
    unsigned char co2_cal;
  } static_data;
  /* Data which is only relevant during power on */
  struct {
    heater_status_t status;            /* current status                      */
    heater_status_t status_sched;      /* next scheduled status if any        */
    unsigned int time;                 /* current sequence time in heater iteration period counts. */
    unsigned short act[NUM_ACT];       /* actuator current values             */
    unsigned short sensor[NUM_SENSOR]; /* current sensor values               */
    unsigned char faults[HT_LAST];     /* amounts of faults for each status   */
    unsigned short wbus_time;          /* wbus command related time in same units as "time" */
    unsigned short cmd_refresh_time;   /* command refresh timeout countere    */
    unsigned char cmd_refresh;         /* current refresh command  */ 
  } volatile_data;
} heater_state_t;

extern
#ifdef __MSP430_169__
__attribute__ ((section (".flashrw")))
#endif
__attribute__ ((aligned(512)))        
union seq_d {
  heater_seqmem_t heater_seq[HT_LAST];
  char force_size[512*4];
} seq_data;

/* Global work buffer */

extern unsigned char wbdata[512];

/*
 * Initialize heater state struct.
 */
void wbus_server_init(heater_state_t *s);

/*
 * Store heater state struct into non-volatile memory.
 */
void wbus_server_store(heater_state_t *s);

/**
 * \brief Decode given W-Bus message, and generate answer based on given
 *        heater state.
 */
int wbus_server_process(unsigned char cmd, unsigned char *data, int *len, heater_state_t *s);

/**
 * \brief add error to error list
 */
void wbus_error_add(heater_state_t *state, unsigned char code, unsigned char n, unsigned char* tmp);

#endif /* __WBUS_SERVER_H__ */


