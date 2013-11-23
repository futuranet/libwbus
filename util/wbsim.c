/*
 * Very minimal wbus device simulator
 */

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
              
#include "wbus.h"
#include "../wbus/wbus_const.h"

struct timeval ti;

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
static const unsigned char id_wbcode[] = { 0xf8, 0x7e, 0x00, 0x02, 0x90, 0xc0, 0x00 }; /* 71 7c c4 e7 3f 80 00*/
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

void handle_ident(unsigned char *data, int *plen)
{
  int i;

  printf("Ident %d\n", data[0]);
  i = data[0]-1;
    
  if (i >= 0 && i < 13) {
    *plen = id[i].size+1;
    memcpy(data+1, id[i].pData, id[i].size);
  } else {
    printf("Unhandled ident\n");
    data[0] = 0x7f;
  }
}

static unsigned char q0[] = { 0x00 };
static unsigned char q2[] = { 0x00, 0x00, 0x00, 0x00, 0x00 }; /* status flags */
static unsigned char q3[] = { 0x00 }; /* subsystem active flags */
static unsigned char q4[] = { 0x1d, 0x3c, 0x3c }; /* Fuel type, max heat time / 10, ventilation time factor */
static unsigned char q5[] = {   70, 0x32,0x00,  0x80, 0x10,0x10,  0x00,0x00 };
static unsigned char q6[] = { 0x00,0x00,  0x01, 0x00,0x00,  0x01, 0x00,0x01 };
static unsigned char q7[] = { 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 };
static unsigned char q10[]= { 0x00, 0x4c, 0x05, 0x00, 0x00, 0x00, 0x00, 0x08,
                              0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00,
                              0x00, 0x00, 0x00, 0x02, 0x11, 0x00, 0x00, 0x00 };
static unsigned char q11[]= { 0x00,0x01,  0x00, 0x00,0x00,  0x01 }; /* PH and SH durations */
static unsigned char q12[]= { 0x00,0x01,  0x00,0x01,  0x00,0x01 }; /* PH, SH and TRS counters */
static unsigned char q15[]= { 0x00, 0x00, 0x00, 0x00, 0x00 };
static unsigned char q17[]= { 0x8a, 0x78 }; /* lower and upper temp threshold */
static unsigned char q18[]= { 0x00,0x00, 0x01 }; /* Ventilation duration */
static unsigned char q19[]= { 0x01,0x00,  0x00,0x10 }; /* prewarming status */
static unsigned char q20[]= { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#define msg(x) { x, sizeof(x) }
static const htdevid_t qa[] =
{
  msg(q0), msg(q0), msg(q2),  msg(q3),  msg(q4),  msg(q5),  msg(q6),  msg(q7),
  msg(q0), msg(q0), msg(q10), msg(q11), msg(q12), msg(q0), msg(q0),  msg(q15),
  msg(q0), msg(q17),msg(q18), msg(q19), msg(q20)
};

void handle_query(unsigned char *data, int *plen)
{
  int q;
  
  q = data[0];
  printf("Query %d\n", q);
 
  switch (q) {
    case 1:
    case QUERY_STATUS0:
    case QUERY_STATUS1:
    case QUERY_OPINFO0:
    case QUERY_SENSORS:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case QUERY_STATUS2:
    case 17:
    case 18:
    case QUERY_FPW:
    case 20:
      memcpy(data+1, qa[q].pData, qa[q].size);
      *plen = qa[q].size+1;
      break;
    default:
      printf("Unknown query code %d\n", q);
      break;
  }   
}

static
unsigned char num_err = 1;

static
err_info_t ht_errors[] = 
{
  /* code, stat, counter, op_state, temp, volts, hours, minutes */
  { 0x1f,
    0,
    1,
    {0x16,5},
    70,
    {0x04,0xbc},
    {0x00,0x01},
    0x05 
  }
};

int error_makelist(unsigned char *data)
{
  int i;
  
  data++;
  *data++ = num_err;
  for (i=0; i<num_err; i++)
  {
    *data++ = ht_errors[i].code;
    *data++ = ht_errors[i].counter;
  }
  return num_err*2+2;
}

int error_makeinfo(unsigned char *data)
{
  int i;
  
  for (i=0; i<num_err; i++)
  {
    if (ht_errors[i].code == data[1]) {
      memcpy(data+1, &ht_errors[i], sizeof(err_info_t));
      break;
    }
  }
  return sizeof(err_info_t)+1;

}

static const unsigned char opinfo[] = { 0x2c, 0x24, 0x25, 0x1c, 0x30, 0xd4, 0xfa, 0x40, 0x74, 0x00, 0x00, 0x63, 0x9c, 0x05 };

void handle_opinfo(unsigned char *data, int *plen)
{
  printf("Operation info %d\n", data[0]);
  
  if (data[0] == OPINFO_LIMITS)
  {
    memcpy(data+1, opinfo, sizeof(opinfo));
    *plen = sizeof(opinfo)+1;
  } else {
    printf("Unhandled opinfo index\n");
  }
}

static int counter = 0;

void handle_error(unsigned char *data, int *plen)
{
  int ecmd;
  
  ecmd = data[0];
  
  printf("Error cmd %d\n", ecmd);
  
  switch (ecmd) {
    case ERR_LIST:
      *plen = error_makelist(data);
      break;
    case ERR_READ:
      *plen = error_makeinfo(data);
      counter++;
      if (counter > 2) {
        ht_errors[0].code ++;
        counter = 0;
      }
      break;
    case ERR_DEL:
      num_err = 0;
      break;
  }
}

static unsigned char cmd_curr = 0;
static unsigned char ts_system_level = 0;

int wbus_handle_msg(unsigned char cmd, unsigned char *data, int *len)
{
  struct timeval tv;
  int err = 0;
  
  gettimeofday(&tv, NULL);
  
  /* Substract initial time */
  tv.tv_sec -= ti.tv_sec;
  tv.tv_usec -= ti.tv_usec;
  if (tv.tv_usec < 0) {
    tv.tv_usec += 1000000;
    tv.tv_sec--;
  }
  printf("[%d:%d]", (int)tv.tv_sec, (int)tv.tv_usec);

  switch (cmd) {
    case WBUS_CMD_OFF:
      printf("Turn off heater\n");
      *len = 1;
      cmd_curr = 0;
      break;
    case WBUS_CMD_ON:
    case WBUS_CMD_ON_PH:
    case WBUS_CMD_ON_VENT:
    case WBUS_CMD_ON_SH:
      printf("Turn on 0x%02x\n", cmd);
      cmd_curr = cmd;
      break;
    case WBUS_CMD_CP:
      printf("Circulation pump external control %d\n", data[0]);
      cmd_curr = cmd;
      break;
    case WBUS_CMD_BOOST:
      printf("Boost mode %d\n", data[0]);
      cmd_curr = cmd;
      break;
    case WBUS_CMD_SL_RD:
      printf("System level read %02x\n", ts_system_level);
      data[2] = ts_system_level;
      *len = 2;
      break;
    case WBUS_CMD_SL_WR:
      ts_system_level = data[0];
      printf("System level write %02x\n", ts_system_level);
      *len = 2; 
      break;
    case WBUS_CMD_U1:
      printf("0x38 (unknown)\n");
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
      printf("Fuel priming\n");
      break;
    case WBUS_CMD_CHK:
      printf("Command check\n");
      if (cmd_curr == data[0]) {
        *len = 1;
        data[0] = 0;
        data[1] = 0x88;
      }
      break;
    case WBUS_CMD_TEST:
      printf("Component check\n");
      break;
    case WBUS_CMD_QUERY:
      handle_query(data, len);
      break;
    case WBUS_CMD_IDENT:
      handle_ident(data, len);
      break;
    case WBUS_CMD_OPINFO:
      handle_opinfo(data, len);
      break;
    case WBUS_CMD_ERR:
      handle_error(data, len);
      break;
    case WBUS_CMD_CO2CAL:
      switch (data[0]) {
        case 1:
          printf("Read CO2 calibration value\n");
          data[1] = 0x97; data[2] = 0x64; data[3] = 0xff;
          *len = 4;
          break;
        case 3:
          printf("Set CO2 calibration value to %02x\n", data[1]);
          *len = 1;
          break;
        default:
          printf("Unknown 0x57 index, %d, length %d\n", data[0], *len);
          break;
      }
      break;
    default:
      printf("0x%02x (unknown), length %d\n", cmd, *len);
      break;
  }
  
  return err;
}

int main(int argc, char *argv[])
{
  HANDLE_WBUS w;
  int err, devidx=0;
  unsigned char data[128];
  unsigned char addr, cmd;
  int len;
 
  /* Get initial time */
  gettimeofday(&ti, NULL);
   
  if (argc > 1)
    devidx = atoi(argv[1]);
   
  err = wbus_open(&w, devidx);
  if (err) {
    printf("wbus_open() failed\n");
    goto bail;
  }
  
  printf("Listening on Wbus %d\n", devidx);
  
  while (1) {
     cmd = 0;
     err = wbus_host_listen(w, &addr, &cmd, data, &len);
     if (err ==  0)
       err = wbus_handle_msg(cmd, data, &len);
     
     err = wbus_host_answer(w, addr, cmd, data, len);  
     if (err) {
       printf("wbus_host_answer() failed\n");
     }
   }

bail:
  return err;
}
