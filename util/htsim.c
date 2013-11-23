/*
 * This program simulates a heater device.
 */

#include "wbus_server.h"
#include "machine.h"

#include <glib.h>
#include <glib/gprintf.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

static
unsigned short model_t0(unsigned short d[2][16])
{
  static float temp_prev = 20;
  static const float K1 = 0.01;

  printf("HE %d\n", d[1][SENSOR_HE]);

  /* heat exchanger temperature */
  if (d[1][SENSOR_HE] > 4000) {
    temp_prev = temp_prev + K1*(110.0-temp_prev)*(((float)(ACTMAX+1)-(float)d[0][ACT_CP])/(float)ACTMAX);
  } else {
    temp_prev = temp_prev + K1*(20.0-temp_prev)*(((float)(ACTMAX+1)-(float)d[0][ACT_CP])/(float)ACTMAX);
  }

  return (int)temp_prev+50;
}

static
unsigned short model_t1(unsigned short d[2][16])
{
  static float temp_prev = 20;
  static const float K1 = 0.002, K2 = 0.001;

  /* heat exchanger temperature */
  if (d[0][ACT_GKPH]) {
    temp_prev = temp_prev + K1*(140.0-temp_prev);
  } else {
    temp_prev = temp_prev + K2*(20.0-temp_prev)*(((float)(ACTMAX+1)-(float)d[0][ACT_DP])/(float)ACTMAX);
  }

  return (int)temp_prev+50;
}

static
unsigned short model_gkph(unsigned short d[2][16])
{
  unsigned short out = 0;
  static float i_prev = 600.0;
  static const float K1 = 0.01;

  if (d[0][ACT_GKPH] > 0) {
    i_prev = i_prev + (150.0-i_prev)*K1;
    out = (unsigned short)i_prev;
  } else {
    i_prev = 600.0;
  }

  return out;  
}
static
unsigned short model_gkz(unsigned short d[2][16])
{
  unsigned short out = 0;
  static float i_prev = 600.0;
  static const float K1 = 0.02;

  if (d[0][ACT_GKZ] > 0) {
    i_prev = i_prev + (150.0-(float)i_prev)*K1;
    out = (unsigned short)i_prev;    
  } else {
    i_prev = 600.0;
  }

  return out;  
}
static
unsigned short model_p(unsigned short d[2][16])
{
  unsigned short out;

  /* ACT is same domain as sensor value. */
  out = d[0][ACT_CC];

  return out;  
}
static
unsigned short model_emf_cf(unsigned short d[2][16])
{
  unsigned short out;

  /* ACT is same domain as sensor value. */
  out = d[0][ACT_CF];

  return out;
}

static
unsigned short model_gpr(unsigned short d[2][16])
{
  static float out_prev = 0.0;
  int flame = 0;
  static const float C1 = 0.0035, C2 = 0.002; /* time constants for raise and decay */
  float stimulus = 0.0;

  /* assuming succesful ingition */
  if (d[0][ACT_DP]) {
    flame = 1;
  }

  if (d[0][ACT_GKZ] > 0 || flame) {
    stimulus = (float)flame + (float)d[0][ACT_GKZ]/(float)ACTMAX;
    if (stimulus > 1.0) {
      stimulus = 1.0;
    }
  }

  /* cheap and dirty Seebeck voltage model */
  out_prev = out_prev + C1*stimulus*(3000-out_prev) + C2*(1.0-stimulus)*(0-out_prev);

  /* amplified and conditioned Seebeck voltage in mV (scaled ADC input) */
  return (unsigned short)out_prev;  
}

static
gboolean cbIterate (gpointer data)
{
  unsigned short (*d)[16] = (unsigned short (*)[16])data;
  int v=0;

  v = d[2][0];

  d[1][SENSOR_T0] = model_t0(d);
  d[1][SENSOR_T1] = model_t1(d);
  d[1][SENSOR_GKPH] = model_gkph(d);
  d[1][SENSOR_GKZ] = model_gkz(d);
  d[1][SENSOR_P] = model_p(d);
  /* d[1][SENSOR_UNUSED0] = model_emf_cf(d); */
  d[1][SENSOR_VCC] = 13800;
  d[1][SENSOR_CAF] = d[0][ACT_CF];
  d[1][SENSOR_GPR] = model_gpr(d);

  /* signal that sensor values are current. */
  if (d[2][0] == 0 && v == 0) {
    d[2][0] = 1;
  }

  return TRUE;
}

int
main(int argc, char **argv)
{
    GMainLoop *ml;
    int shmid;
    unsigned short (*data)[16];

    shmid = shmget(0xb0E1, 4096, 0666 | IPC_CREAT);
    if (shmid == -1) {
      g_message("shmget() failed");
      return -1;
    }
    data = shmat(shmid, (void *)0, 0);
    if ((int)data == -1) {
      g_message("shmat() failed");
      return -1;
    }

    g_timeout_add(256, cbIterate, data);

    ml = g_main_loop_new (NULL, FALSE);        

    g_main_loop_run(ml);
    
    g_main_loop_unref(ml);

    return 0;
}
