/*
 * Wbus device server (heater device end point)
 */

#include "wbus_server.h"

#include "wbus.h"
#include "wbus_const.h"
#include "machine.h"

#include <string.h>

/* global data */
unsigned char wbdata[512];


/*  DEVICE ID DATA */

typedef struct {
  const unsigned char *pData;
  const unsigned char size;
} htdevid_t;

static const unsigned char id_dev[] = { 0x00, 0x01, 0x02, 0x03, 0x04 };
                                        /*09 00 83 75 46*/
static const unsigned char id_ver[] = { 0x33, 0x03, 
                                        0x03, 0x48, 0x04, 0x05, 0x39, /* day of week / week / year // ver/ver */
                                        0x03, 0x48, 0x04, 0x05, 0x39 };
static const unsigned char id_dataset[] = { 0x09, 0x00 ,0x00 ,0x00 ,0x00 ,0x00 }; /* 09 00 79 78 58 07 */
static const unsigned char id_dom_cu[] = { 19, 06, 9 };
static const unsigned char id_dom_ht[] = { 01, 01, 01};
static const unsigned char id_tscode[] = { 0xa2, 0x4f, 0x06, 0xd1, 0x04, 0x24, 0x10, 0x04 }; /* a2 4f 06 d1 04 24 10 04 */
static const unsigned char id_custid[] = { 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, };
static const unsigned char id_u1[] = { 00 };
static const unsigned char id_serial[] = { 0x00, 0x00, 0x64, 0x27, 0x48, 0x43, 0x42 };
static const unsigned char id_wbver[] = { 0x33 };
static const unsigned char id_name[] = { 'P', 'O', 'E', 'L', 'I', ' ', ' ', ' ' };
static const unsigned char id_wbcode[] = { 0xf8, 0x7e, 0x20, 0x02, 0x10, 0xc0, 0x00 }; /* 71 7c c4 e7 3f 80 00*/
static const unsigned char id_swid[] = { 00, 00, 00, 00, 30 };

static const htdevid_t id[] = 
{ 
  { id_dev, sizeof(id_dev) },
  { id_ver, sizeof(id_ver) },
  { id_dataset, sizeof(id_dataset) },
  { id_dom_cu, sizeof(id_dom_cu) },
  { id_dom_ht, sizeof(id_dom_ht) },
  { id_tscode, sizeof(id_tscode) },
  { id_custid, sizeof(id_custid) },
  { id_u1, sizeof(id_u1) },
  { id_serial, sizeof(id_serial) },
  { id_wbver, sizeof(id_wbver) },
  { id_name, sizeof(id_name) },
  { id_wbcode, sizeof(id_wbcode) },
  { id_swid, sizeof(id_swid) }
};

static
void handle_ident(unsigned char *data, int *plen)
{
  int i;

  /*PRINTF("Ident %d\n", data[0]);*/
  i = data[0]-1;
    
  if (i >= 0 && i < 13) {
    *plen = id[i].size+1;
    memcpy(data+1, id[i].pData, id[i].size);
  } else {
    PRINTF("Unhandled ident\n");
    data[0] = 0x7f;
  }
}

/* DEVICE OPERATING DATA */

/* Some global default sensor limits  */

#define MV_MIN 11000 /* Supply min 11000 miliVolt */
#define MV_MAX 16000 /* Supply max 16000 miliVolt */
#define TEMP_MIN  (0)      /* -50°C */
#define TEMP_MAX  (90+50)  /*  90°C */
#define TEMP_UP   (50+50)  /*  ramp up trip point */
#define TEMP_DOWN (80+50)  /*  ramp down trip point */

#define TEMPN_MIN (0)      /* -50°C (nozzle stock) */
#define TEMPN_MAX (140+50) /* 140°C (nozzle stock) */
#define IGK_MAX  3200  /* Max glow plug current in 1/10 mA */
#define IGK_MIN    50  /* Min glow plug current in 1/10 mA */
#define IGK_NOISE 100  /* glow plug current measure noise 1/10 mA */
#define IPH_MAX  3200  /* Max nozzle stock preheating current in 1/10 mA */
#define IPH_MIN    10  /* Min nozzle stock preheating current in 1/10 mA */
#define IPH_NOISE 100  /* nozzle stock preheating current measure noise 1/10 mA */

#if defined(CF_DBW46)

#define CAF_FL 2300
#define CAF_PL 1400
#define CAF_SB  300
#define CAF_IG  300
#elif defined(CF_TOYBLOWER)
#define CAF_FL 2300*3
#define CAF_PL 1600*3
#define CAF_SB 1000*3
#define CAF_IG  250*3
#endif

#if (NOZZLE == 1) /* Hago SN609-02 */
#define CC_FL 55 /*63*/
#define CC_PL 22
#define CC_SB  8
#define CC_IG  4
#elif (NOZZLE == 2) /* Delavan SN30609-01*/
#define CC_FL 50
#define CC_PL 20
#define CC_SB  3
#define CC_IG  3
#else
#error NOZZLE not defined correctly. 1: Hago 02, 2: Delavan 01
#endif

/* 0.033ml/pulse and 9.7KW/h per Liter yield: 2.5Hz -> 2.88KW, 4.6Hz -> 4.95KW, 5Hz -> 5.76 KW*/
#define DP_FL  86
#define DP_PL  50
#define DP_SB  40
#define DP_IG  43

#define IGKZ_NOISE 500 /* Z glow plug current measure noise due to voltage measuring */
#define P_NOISE      8 /* Pressure sensor noise */
#define P_MAX     (100<<ACTMAX_LD2) /* Max nozzle air pressure in ACTMAX domain scale (max 1Bar)*/
#define OVERHEAT_OK  1 /* Overheating fuse status */
#define HE_MAX    6000 /* Heating Energy limit */

#define CAF_MAX  65535 /* Combustion air fan max RPM */

/* Predefined heater run sequence */
/* Actuator : ACT_AUX ACT_DP ACT_CP ACT_NONE ACT_VF ACT_CC ACT_CF ACT_GKZ ACT_NSPH */
/* Units    : %       Hz/20  %      -        %      cBar   RPM    %       %        */

/* Sensors : GPR T0 T1 NSPH GKZ P    OverHeat VCC CAF HE   FD */
/* Units   : Ohm °C °C   mA  mA cBar      0/1 mV  RPM  W  0/1 */

/* convert mili seconds into amount of heater period iterations */
#define MSEC2PERIODS(t) ((long)(( (JFREQ/HEATER_PERIOD) * (t) )/1000L))
#define ACT_STEP(e,s,t) (((long)((e)-(s))<<STEP_SCALE)/MSEC2PERIODS(t))
#define ACT_STEP_CAF(e,s,t) (((long)((e)-(s))<<(STEP_SCALE-ACTMAX_LD2))/MSEC2PERIODS(t))

#define SEQDEF(t, s0,e0, s1,e1, s2,e2, s3,e3, s4,e4, s5,e5, s6,e6, s7,e7, s8,e8) \
  .seq.time = MSEC2PERIODS(t), \
  .seq.act_target = { e0,e1,e2,e3,e4,e5,((e6)>>ACTMAX_LD2),e7,e8 }, \
  .seq.act_step = { ACT_STEP(e0,s0,t),     ACT_STEP(e1,s1,t), ACT_STEP(e2,s2,t), \
                    ACT_STEP(e3,s3,t),     ACT_STEP(e4,s4,t), ACT_STEP(e5,s5,t), \
                    ACT_STEP_CAF(e6,s6,t), ACT_STEP(e7,s7,t), ACT_STEP(e8,s8,t) },

/*Note: the seq_data array order below must match the enum heater_status_t */
#ifdef __MSP430__
__attribute__ ((section (".flashrw")))
#endif
__attribute__ ((aligned(512)))
union seq_d seq_data = {
 {
  { /* HT_OFF 0 */
    .seq.status_next = HT_OFF,
    SEQDEF(1000L, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0)  
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,       0, 0, MV_MIN,       0, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, P_NOISE, 0, MV_MAX, CAF_MAX, 0, 0 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF, HT_OFF,    HT_OFF,    HT_OFF, HT_LOCKED, HT_OFF, HT_OFF, HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_OFF, HT_OFF, HT_LOCKED, HT_LOCKED, HT_OFF, HT_LOCKED, HT_OFF, HT_OFF, HT_OFF, HT_OFF },
    .seq.max_faults = 0,
    .seq.fault_mask = 0xff /* as faults may lead here, we need to mask all errors to avoid error storms. */
  },
  { /* HT_START 1 */
    .seq.status_next = HT_PREHEAT,
    SEQDEF(3000L, 0,0, 0,0, 50,0, 0,0, 0,0, 0,0, 1000,1000, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,     0, 0, MV_MIN, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, P_MAX, 0, MV_MAX, CAF_MAX, 0, 0 },
    .seq.on_sensor_min = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 0,
    .seq.fault_mask = 0
  },
  { /* HT_PREHEAT 2 */
    .seq.status_next = HT_GLOW,
    SEQDEF( 3* 60* 1000L, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 100,100)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN, IPH_MIN,          0,       0, 0, MV_MIN,       0, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX,     80+50, IPH_MAX, IGKZ_NOISE, P_NOISE, 0, MV_MAX, CAF_MAX, 0, 0 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF,  HT_OFF, HT_OFF, HT_OFF, HT_LOCKED, HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_OFF, HT_GLOW, HT_OFF, HT_OFF, HT_OFF, HT_LOCKED, HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 0,
    .seq.fault_mask = (1<<SENSOR_T1)
  },
  { /* HT_GLOW 3 */
    .seq.status_next = HT_IGNITE,
    SEQDEF(20*1000L, 0,0, 1,1, 0,0, 0,0, 0,0, 0,0, 0,0, 100,100, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN,     00+50,         0, IGK_MIN,       0, 0, MV_MIN,       0, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGK_MAX, P_NOISE, 0, MV_MAX, CAF_MAX, 0, 0 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF, HT_OFF, HT_OFF, HT_OFF, HT_LOCKED, HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_OFF, HT_OFF, HT_OFF, HT_OFF, HT_OFF, HT_LOCKED, HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 0,
    .seq.fault_mask = 0
  },
  { /* HT_IGNITE 4 */
    .seq.status_next = HT_STABILIZE,
    SEQDEF(40000L, 0,0, DP_IG,DP_SB, 100,100, 0,0, 0,0, CC_IG,CC_SB, CAF_IG,CAF_SB, 100,100, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN,     00+50,         0, IGK_MIN,   0, 0, MV_MIN,       0,      0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGK_MAX, 200, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_OFF, HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 2,
    .seq.fault_mask = 0
  },
  { /* HT_STABILIZE 5 */
    .seq.status_next = HT_BURN_L,
    SEQDEF(40000L, 0,0, DP_SB,DP_PL, 100,100, 0,0, 0,0, CC_SB,CC_PL, CAF_SB,CAF_PL, 100,100, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN,     00+50,         0, IGK_MIN,  00, 0, MV_MIN,       0,      0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGK_MAX, 500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_STOP,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 5,
    .seq.fault_mask = 0
  },
  { /* HT_RAMPUP 6 */
    .seq.status_next = HT_BURN_H,
    SEQDEF(20000L, 0,0, DP_PL,DP_FL, 100,100, 0,0, 0,0, CC_PL,CC_FL, CAF_PL,CAF_FL, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,   0, 0, MV_MIN,       0,      0, 1 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, 500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_STOP,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 20,
    .seq.fault_mask = 0
  },
  { /* HT_BURN_H 7 */
    .seq.status_next = HT_STOP,
    SEQDEF(60*60*1000L, 0,0, DP_FL,DP_FL, 100,100, 0,0, 100,100, CC_FL,CC_FL, CAF_FL,CAF_FL, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,   0, 0, MV_MIN,       0,      0, 1 },
    .seq.sensor_max = { 3300, TEMP_DOWN,TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, 500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF,     HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_RAMPDOWN,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 10,
    .seq.fault_mask = (1<<SENSOR_T0)
  },
  { /* HT_RAMPDOWN 8 */
    .seq.status_next = HT_BURN_L,
    SEQDEF(20000L, 0,0, DP_FL,DP_PL, 100,100, 0,0, 100,100, CC_FL,CC_PL, CAF_FL,CAF_PL, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,   0, 0, MV_MIN,       0,      0, 1 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, 500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF, HT_OFF, HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF  },
    .seq.on_sensor_max = { HT_OFF, HT_STOP,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 20,
    .seq.fault_mask = 0
  },
  { /* HT_BURN_L 9 */
    .seq.status_next = HT_STOP,
    SEQDEF(60*60*1000L, 0,0, DP_PL,DP_PL, 100,100, 0,0, 100,100, CC_PL,CC_PL, CAF_PL,CAF_PL, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_UP, TEMPN_MIN,         0,          0,   0, 0, MV_MIN,       0,      0, 1 },
    .seq.sensor_max = { 3300, TEMP_MAX,TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, 500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF, HT_RAMPUP,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF, HT_STOP,  HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 10,
    .seq.fault_mask = (1<<SENSOR_T0)
  },
  { /* HT_STOP 10 */
    .seq.status_next = HT_COOLDOWN,
    SEQDEF(20000L, 0,0, 0,0, 100,100, 0,0, 0,0, CC_PL,CC_PL, CAF_PL,CAF_PL, 0,0, 0,0)
    .seq.sensor_min = {    0,    25+50, TEMPN_MIN,         0,          0,   0, 0, MV_MIN,       0,      0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, 500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF,HT_COOLDOWN,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF,HT_COOLDOWN,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 5,
    .seq.fault_mask = (1<<SENSOR_T0)
  },
  { /* HT_COOLDOWN 11 */
    .seq.status_next = HT_END,
    SEQDEF(60*1000L, 0,0, 0,0, 100,100, 0,0, 0,0, CC_PL,0, 600,600, 0,0, 0,0)
    .seq.sensor_min = {    0,       40+50, TEMPN_MIN,         0,          0,    0, 0, MV_MIN,       0,      0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX+20, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE,  500, 0, MV_MAX, CAF_MAX, HE_MAX, 1 },
    .seq.on_sensor_min = { HT_OFF,HT_END,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 5,                    
    .seq.fault_mask = (1<<SENSOR_T0)
  },
  { /* HT_END 12 */
    .seq.status_next = HT_OFF,
    SEQDEF(3000L, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,       0, 0, MV_MIN,       0,      0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, P_NOISE, 0, MV_MAX, CAF_MAX,      0, 0 },
    .seq.on_sensor_min = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 5,                    
    .seq.fault_mask = 0
  },
  { /* HT_VENT 13 */
    .seq.status_next = HT_OFF,
    SEQDEF(60*1000L, 0,0, 0,0, 0,0, 0,0, 100,100, 0,0, 0,0, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,       0, 0, MV_MIN, 0, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, P_NOISE, 0, MV_MAX, 0, 0, 0 },
    .seq.on_sensor_min = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 0,                    
    .seq.fault_mask = 0 
  },
  { /* HT_TEST 14 */
    .seq.status_next = HT_OFF,
    SEQDEF(30*1000L, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,       0,      0,     0, 0, MV_MIN,       0, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_MAX,IGK_MAX, P_MAX, 0, MV_MAX, CAF_MAX, 0, 0 },
    .seq.on_sensor_min = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.on_sensor_max = { HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_OFF,HT_LOCKED,HT_OFF,HT_OFF,HT_OFF, HT_OFF },
    .seq.max_faults = 0,                    
    .seq.fault_mask = 0 
  },
  { /* HT_LOCKED 15 */
    .seq.status_next = HT_LOCKED,
    SEQDEF(2*60*1000L, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0)
    .seq.sensor_min = {    0, TEMP_MIN, TEMPN_MIN,         0,          0,       0, 0, MV_MIN, 0, 0, 0 },
    .seq.sensor_max = { 3300, TEMP_MAX, TEMPN_MAX, IPH_NOISE, IGKZ_NOISE, P_NOISE, 1, MV_MAX, 0, 0, 0 },
    .seq.on_sensor_min = { HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED },
    .seq.on_sensor_max = { HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED,HT_LOCKED },
    .seq.max_faults = 0,                    
    .seq.fault_mask = 0xff /* as faults may lead here, we need to mask all errors to avoid error storms. */ 
  }
 } 
}; 

static
void dataset_read(heater_seqmem_t * seq, heater_status_t s)
{ 
  memcpy(seq, &seq_data.heater_seq[s], sizeof(heater_seq_t));
}

static
void dataset_write(heater_seqmem_t * seq, heater_status_t s)
{  
  /* Temporal buffer for flash segment write. static because
     it wont fit on the task stack */
  int addr = (int)(unsigned char *)&seq_data.heater_seq[s]; 
  int offset;
  unsigned char *base;

  base = (unsigned char *)((addr) & (~(int)0x1ff));
  offset = addr & 0x1ff;

  /* prepare memory segment */
  memmove(wbdata+offset, seq, sizeof(heater_seq_t));
  /* copy other flash data that should not be changed */
  memcpy(wbdata, (unsigned char*)base, offset);
  memcpy(wbdata+offset+sizeof(heater_seqmem_t), (unsigned char*)base+offset+sizeof(heater_seqmem_t), 512-offset-sizeof(heater_seqmem_t));
  /* Write segment back to flash */
  flash_write(base, wbdata, 512); 
}


static const unsigned char q4[] = { 0x00, 0x06, 0x03, 0x00 }; /* Fuel type, max heat time / 10, ventilation time factor */

/* Translate wb_server state into WBus state */
static
unsigned char ht_state2wb_state(heater_state_t *s)
{
  unsigned char wbstate;

  switch (s->volatile_data.status) {
    case HT_OFF:
      wbstate = WB_STATE_OFF;
      break;
    case HT_START:
      wbstate = WB_STATE_START;
      break;
    case HT_PREHEAT:
      wbstate = WB_STATE_PRES;
      break;
    case HT_GLOW:
      wbstate = WB_STATE_GLOW;
      break;
    case HT_IGNITE:
      wbstate = WB_STATE_IGN;
      break;
    case HT_STABILIZE:
      wbstate = WB_STATE_STAB;
      break;
    case HT_RAMPUP:
      wbstate = WB_STATE_LCHGUP;
      break;
    case HT_RAMPDOWN:
      wbstate = WB_STATE_LCHGDN;
      break;
    case HT_BURN_L:
      wbstate = WB_STATE_CPL;
      break;
    case HT_BURN_H:
      wbstate = WB_STATE_CFL;
      break;
    case HT_STOP:
      wbstate = WB_STATE_BO;
      break;
    case HT_COOLDOWN:
      wbstate = WB_STATE_COOL;
      break;
    case HT_END:
      wbstate = WB_STATE_OFFR;
      break;
    case HT_VENT:
      wbstate = WB_STATE_VENT;
      break;
    case HT_TEST:
      wbstate = WB_STATE_CTRL;
      break;
    case HT_LOCKED:
      wbstate = WB_STATE_LOCK;
      break; 
    default:
      wbstate = WB_STATE_OFF;
      break;
  }
  return wbstate;
}

static
void handle_query(unsigned char *data, int *plen, heater_state_t *s)
{
  int q;
  unsigned short *acts = s->volatile_data.act;
  unsigned short *sensors = s->volatile_data.sensor;

  q = data[0];
  /*PRINTF("Query %d\n", q);*/
  data++;
 
  switch (q) {
    case QUERY_STATUS0:
      data[0]=0; data[1]=0; data[2]=0; data[3]=0; data[4]=0; *plen=6;
      break;
    case QUERY_STATUS1:
      data[0] = 0;
      if (acts[ACT_CF] != 0)
        data[0] |= STA10_CF;
      if (acts[ACT_GKZ] != 0)
        data[0] |= STA10_GP;
      if (acts[ACT_DP] != 0)
        data[0] |= STA10_FP;
      if (acts[ACT_CP] != 0)
        data[0] |= STA10_CP;
      if (acts[ACT_VF] != 0)
        data[0] |= STA10_VF;
      if (acts[ACT_GKPH] != 0)
        data[0] |= STA10_NSH;
      if (sensors[SENSOR_FD] != 0)
        data[0] |= STA10_FI;
      *plen = 2;
      break;
    case QUERY_OPINFO0:
      memcpy(data, q4, 4);
      *plen = 4;
      break;
    case QUERY_SENSORS:
      data[SEN_TEMP] = (unsigned char)sensors[SENSOR_T0];
      VOLT2WORD(data[SEN_VOLT], sensors[SENSOR_VCC]);
      data[SEN_FD] = sensors[SENSOR_FD]; /* Flag */
      //WATT2WORD(data[SEN_HE], sensors[SENSOR_HE]);
#if (ACTMAX_LD2 != 2)
      WATT2WORD(data[SEN_HE], (sensors[SENSOR_P]>>ACTMAX_LD2)*10);
#else
      WATT2WORD(data[SEN_HE], (sensors[SENSOR_P]<<1) + (sensors[SENSOR_P]>>1));
#endif
      OHM2WORD(data[SEN_GPR], sensors[SENSOR_GPR]); /* Flame detector */
      *plen = 9;
      break;
    case QUERY_COUNTERS1:
      HOUR2WORD(data[0], s->static_data.working_duration.hours);
      data[2] = s->static_data.working_duration.minutes;
      HOUR2WORD(data[3], s->static_data.operating_duration.hours);
      data[5] = s->static_data.operating_duration.minutes;
      SWAP(&data[6], s->static_data.counter);
      *plen = 9;
      break; 
    case QUERY_STATE:
      data[OP_STATE] = ht_state2wb_state(s);
      data[OP_STATE_N] = 0;
      data[DEV_STATE] = 0;
      data[4] = 0; data[5] = 0; data[6] = 0;
      *plen = 7;
      break;
    case QUERY_DURATIONS0:
      { 
        int i;
        
        for (i=0; i<24; i++) data[i] = i;
      }
      *plen = 25;
      break;
    case QUERY_DURATIONS1:
      data[0] = 1; data[1] = 2; data[2] = 3; data[3] = 4; data[4] = 5; data[5] = 6;
      *plen = 7;
      break;
    case QUERY_COUNTERS2:
      SWAP(&data[STA3_SCPH], s->static_data.counter);
      SWAP(&data[STA3_SCSH], 0);
      SWAP(&data[STA34_FD], 0); /* ??? */
      *plen = 7;
      break;
    case QUERY_STATUS2:
      /* data[STA2_GP] = acts[ACT_GKZ]>>(ACTMAX_LD2-1); */
      data[STA2_GP] = mult_u16xu16h(sensors[SENSOR_GKZ], 65536/100); /* in 10mA, scaled to A */
      data[STA2_FP] = acts[ACT_DP];
      /* data[STA2_CAF] = acts[ACT_CF]>>(ACTMAX_LD2-1); */
#if 0
      /* CAF as percentage (WBUS code WB_CODE_CAVR bit not set) */
      data[STA2_CAF] = sensors[SENSOR_CAF]; /* percentage */
      data[STA2_U0] = 0;
#else
      /* CAF as RPM (WBUS code WB_CODE_CAVR bit set) */
      RPM2WORD(data[STA2_CAF], sensors[SENSOR_CAF]); /* RPM */
#endif
      data[STA2_CP] = acts[ACT_CP]>>(ACTMAX_LD2-1);
      *plen = 6;
      break;
    case QUERY_OPINFO1:
      data[0] = 0x78;
      data[1] = 0x6e;
      data[2] = 0x00;
      *plen = 4;
      break;
    case QUERY_DURATIONS2:
      /* Ventilation duration mms */
      data[0] = 0; data[1] = 0; data[2] = 0;
      *plen = 4;
      break;
    case QUERY_FPW:
      /* Note: sensors[SENSOR_GKPH] is scaled by factor 10 of real value. */
#if 0
      /* Be WBUS 3.3 compliant and return fuel prewarming heater resistance. */
      OHM2WORD(data[FPW_R], (acts[ACT_GKPH]>0) ? div_u32_u16(sensors[SENSOR_VCC]*100L, sensors[SENSOR_GKPH]) : 0);
#else
      /* Display fuel prewarming temperature (more useful, but not WBUS 3.3 compliant) */
      OHM2WORD(data[FPW_R], (signed)sensors[SENSOR_T1] - 50);
#endif
      /* 10*65536*65536/1000000 = 42949.67296 */
      WATT2WORD(data[FPW_P], mult_u16xu16h(mult_u16xu16h(sensors[SENSOR_GKPH], sensors[SENSOR_VCC]), 42950));
      *plen = 5;
      break;
    case 20:
      data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0; data[4] = 0; data[5] = 0; data[6] = 0;
      *plen = 8;
      break;
    default:
      PRINTF("Unknown query code %d\n", q);
      break;
  }   
}

static
#ifdef __MSP430__
__attribute__ ((section (".infomem")))
#endif
unsigned char opinfo[] = { 0x2c, 0x24, 0x25, 0x1c, 0x30, 0xd4, 0xfa, 0x40, 0x74, 0x00, 0x00, 0x63, 0x9c, 0x05 };

static
void handle_opinfo(unsigned char *data, int *plen, heater_state_t *s)
{
  PRINTF("Operation info %d\n", data[0]);
  
  if (data[0] == OPINFO_LIMITS)
  {
    memcpy(data+1, opinfo, sizeof(opinfo));
    *plen = sizeof(opinfo)+1;
  } else {
    PRINTF("Unhandled opinfo index\n");
  }
}


#define MAX_ERR 11

static
#ifdef __MSP430__
__attribute__ ((section (".infomem")))
#endif
union {
  err_info_t ht_errors[MAX_ERR]; 
  unsigned char fill[128];
} union_error = {
  {
  { ERR_NOSTART, 0x03, 1, { WB_STATE_PH, WB_DSTATE_STFL }, 68, { 0x30, 0xd4 }, { 0, 1 }, 3 }
  }
};

static
int error_makelist(unsigned char *data)
{
  int i;
  
  for (i=0; i<MAX_ERR; i++)
  {
    if (union_error.ht_errors[i].code != 0) {
      data[2+i*2] = union_error.ht_errors[i].code;
      data[3+i*2] = union_error.ht_errors[i].counter;
    } else {
      break;
    }
  }
  data[1] = i;
  return i*2+2;
}

static
int error_makeinfo(unsigned char *data)
{
  int i;
  
  for (i=0; i<MAX_ERR; i++)
  {
    if (union_error.ht_errors[i].code == data[1]) {
      memcpy(data+1, &union_error.ht_errors[i], sizeof(err_info_t));
      break;
    }
  }
  return 12;
}


void wbus_error_add(heater_state_t *state, unsigned char code, unsigned char n, unsigned char *tmp)
{
  err_info_t *e = (err_info_t*)tmp;
  unsigned short *s = state->volatile_data.sensor;
  int i;
 
  /* code, stat, counter, op_state, temp, volts, hours, minutes */
  for (i=0; i<MAX_ERR; i++)
  {
    if (union_error.ht_errors[i].code == 0 || union_error.ht_errors[i].code == code)
    {
      memcpy(e, union_error.ht_errors, sizeof(union_error.ht_errors));
      e[i].code = code;
      e[i].flags = 3;
      e[i].counter++;
      e[i].op_state[0] = ht_state2wb_state(state);
      e[i].op_state[1] = n;
      e[i].temp = s[SENSOR_T0];
      VOLT2WORD(e[i].volt[0], s[SENSOR_VCC]);
      e[i].hour[1] = state->static_data.working_duration.hours;
      e[i].minute = state->static_data.working_duration.minutes;
      /* FIXME: takes way too long. */
      flash_write(union_error.ht_errors, e, sizeof(union_error.ht_errors));
      break;
    } 
  }
}

static
void handle_error(unsigned char *data, int *plen, heater_state_t *s)
{
  int ecmd;
  
  ecmd = data[0];
  
  /* PRINTF("Error cmd %d\n", ecmd); */
  
  switch (ecmd) {
    case ERR_LIST:
      *plen = error_makelist(data);
      break;
    case ERR_READ:
      *plen = error_makeinfo(data);
      break;
    case ERR_DEL:
      /* Erase flash page containing error codes */
      flash_write((void*)union_error.ht_errors, NULL, sizeof(union_error.ht_errors));
      break;
  }
}

int wbus_server_process(unsigned char cmd, unsigned char *data, int *len, heater_state_t *s)
{
  int err = 0;

  /* PRINTF("len = %d cmd = %x idx = %x\n", *len, cmd, data[0]); */

  switch (cmd) {
    case WBUS_CMD_OFF:
      if (s->volatile_data.status != HT_OFF 
       && s->volatile_data.status != HT_COOLDOWN
       && s->volatile_data.status != HT_STOP)
      {
        s->volatile_data.time = 0;
        s->volatile_data.cmd_refresh_time = 0;
        if (s->volatile_data.status == HT_VENT) {
          s->volatile_data.status_sched = HT_OFF;
        } else {
          s->volatile_data.status_sched = HT_STOP;
        }
      }
      *len = 1;
      break;
    case WBUS_CMD_ON:
    case WBUS_CMD_ON_PH:
    case WBUS_CMD_ON_SH:
    case WBUS_CMD_ON_VENT:
      if (s->volatile_data.status == HT_OFF) {
        s->volatile_data.time = 0;
        s->volatile_data.status_sched = (cmd == WBUS_CMD_ON_VENT) ? HT_VENT : HT_START;
        s->volatile_data.wbus_time = data[0] * (60*JFREQ/HEATER_PERIOD);
        s->volatile_data.cmd_refresh_time = MSEC2PERIODS(CMD_REFRESH_PERIOD);
        s->volatile_data.cmd_refresh = cmd;
      }
      break;
    case WBUS_CMD_CP:
    case WBUS_CMD_BOOST:
      /* ToDo */
      break;
    case WBUS_CMD_U1:
      PRINTF("0x38 (unknown)\n");
      *len = 7;
      data[0] = 0xb8;
      data[1] = 0x0b;
      data[2] = 0x00;
      data[3] = 0x00;
      data[4] = 0x00;
      data[5] = 0x03;
      data[6] = 0xdd;
      err = 1;
      break;
    case WBUS_CMD_FP:
      PRINTF("Fuel priming (%d) %d seconds\n", data[0], ((data[1]<<8)+data[2])<<1);
      if (s->volatile_data.status == HT_OFF) {
        s->volatile_data.act[ACT_DP] = 40; /* Set dosing pump to 2 Hz */
        s->volatile_data.status_sched = HT_TEST;
        s->volatile_data.wbus_time = ((((int)data[1]<<9)+((int)data[2]<<1))) * (JFREQ/HEATER_PERIOD);
        s->volatile_data.time = 0;
      }
      break;
    case WBUS_CMD_CHK:
      if (s->volatile_data.cmd_refresh == data[0]) {
        s->volatile_data.cmd_refresh_time = MSEC2PERIODS(CMD_REFRESH_PERIOD);
        data[0] = 0;
      } else {
        data[0] = 1;
      }
      *len = 1;
      break;
    case WBUS_CMD_TEST:
      if (s->volatile_data.status == HT_OFF) {
        unsigned short *acts = s->volatile_data.act;
        unsigned short tmp;
        
        s->volatile_data.status_sched = HT_TEST;
        s->volatile_data.wbus_time = data[1] * (JFREQ/HEATER_PERIOD);
        s->volatile_data.time = 0;

        tmp = ((unsigned short)data[2]<<8) | (unsigned short)data[3];
        switch (data[0]) {
          case WBUS_TEST_CF:  acts[ACT_CF]  = tmp; break;
          case WBUS_TEST_FP:  acts[ACT_DP]  = tmp; break;
          case WBUS_TEST_GP:  acts[ACT_GKZ] = tmp<<(ACTMAX_LD2-1); break;
          case WBUS_TEST_CP:  acts[ACT_CP]  = tmp<<(ACTMAX_LD2-1); break;
          case WBUS_TEST_VF:  acts[ACT_VF]  = 100<<ACTMAX_LD2; break;
          case WBUS_TEST_SV:  acts[ACT_AUX] = 100<<ACTMAX_LD2; break;
          case WBUS_TEXT_NAC: acts[ACT_CC]  = tmp<<(ACTMAX_LD2-1); break;
          case WBUS_TEST_NSH: acts[ACT_GKPH] = 100<<ACTMAX_LD2; break;
          case WBUS_TEST_FPW: acts[ACT_GKPH] = tmp<<ACTMAX_LD2; break;
        }
      }
      if (s->volatile_data.status == HT_TEST && data[0] == 0) {
        s->volatile_data.status_sched = HT_OFF;
        s->volatile_data.time = 0;
      }
      break;
    case WBUS_CMD_QUERY:
      handle_query(data, len, s);
      break;
    case WBUS_CMD_IDENT:
      handle_ident(data, len);
      break;
    case WBUS_CMD_OPINFO:
      handle_opinfo(data, len, s);
      break;
    case WBUS_CMD_ERR:
      handle_error(data, len, s);
      break;
    case WBUS_CMD_CO2CAL:
      switch (data[0]) {
        case CO2CAL_READ:
          PRINTF("Read CO2 calibration value\n");
          data[1] = s->static_data.co2_cal; data[2] = 0x00; data[3] = 0xff;
          *len = 4;
          break;
        case CO2CAL_WRITE:
          PRINTF("Set CO2 calibration value to %02x\n", data[1]);
          *len = 1;
          s->static_data.co2_cal = data[1];
          break;
        default:
          PRINTF("Unknown CO2 calibration (0x57) index, %d, length %d\n", data[0], *len);
          break;
      }
      break;
    case WBUS_CMD_DATASET:
      switch (data[0]) {
        case DATASET_COUNT:
          data[1] = HT_LAST;
          *len = 2;
          break;
        case DATASET_READ:
          dataset_read((heater_seqmem_t*)&data[2], data[1]);
          *len = sizeof(heater_seq_t)+2;
          break;
        case DATASET_WRITE:
          PRINTF("data set write %d\n", data[1]);
          dataset_write((heater_seqmem_t*)&data[2], data[1]);
          *len = sizeof(heater_seq_t)+2;
          break;
      }
      break;
    default:
      PRINTF("0x%02x (unknown), len=%d, param1=%d, param2=%d\n", cmd, *len, data[0], data[1]);
      break;
  }
  
  return err;
}

void wbus_server_init(heater_state_t *s)
{
  s->static_data.working_duration.seconds = 0;  
  s->static_data.working_duration.minutes = 0;  
  s->static_data.working_duration.hours = 0;  
  s->static_data.operating_duration.seconds = 0;
  s->static_data.operating_duration.minutes = 0;
  s->static_data.operating_duration.hours = 0;
  s->static_data.counter = 0;
  s->static_data.co2_cal = 0x80;
}

