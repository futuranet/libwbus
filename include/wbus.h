/*
 * W-Bus access interface
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#ifndef __WBUS_H__
#define __WBUS_H__

//#define WBUS_TEST
#define WBUS_HOST

typedef struct
{
  unsigned char wbus_ver;
  char dev_name[9];
  unsigned char wbus_code[7];
  unsigned char strange[7];
  unsigned char strange2[1];
  unsigned char dev_id[5];
  unsigned char data_set_id[6];
  unsigned char sw_id[6];
  unsigned char hw_ver[2]; /* week / year */
  unsigned char sw_ver[5]; /* day of week / week / year // ver/ver */
  unsigned char sw_ver_eeprom[5];
  unsigned char dom_cu[3]; /* day / week / year */
  unsigned char dom_ht[3];
  unsigned char customer_id[20]; /* Vehicle manufacturer part number */
  unsigned char serial[6];
  unsigned char u0[7];
  /* unsigned char test_signature[5]; */
} wb_info_t;

typedef wb_info_t *HANDLE_WBINFO;

#define WB_NUM_SENSORS 0x14
#define WB_IDENT_LINES 16

typedef struct
{
  unsigned char length;
  unsigned char idx;
  unsigned char value[32];
} wb_sensor_t;

typedef wb_sensor_t *HANDLE_WBSENSOR;

typedef struct {
  unsigned char code;
  unsigned char flags;
  unsigned char counter;
  unsigned char op_state[2];
  unsigned char temp;
  unsigned char volt[2];
  unsigned char hour[2];
  unsigned char minute;
} err_info_t;

typedef struct {
  unsigned char nErr;
  struct {
    /*
    unsigned char code;
    unsigned char count;
    */
    err_info_t info;
  } errors[32];
} wb_errors_t;
typedef wb_errors_t *HANDLE_WBERR;

typedef struct WBUS *HANDLE_WBUS;

/* Overall handling stuff */
int wbus_open(HANDLE_WBUS *pWbus, unsigned char dev_idx);
void wbus_close(HANDLE_WBUS wbus);

/* Low level W-Bus I/O */
int wbus_io( HANDLE_WBUS wbus,
             unsigned char cmd,
             unsigned char *out,
             unsigned char *out2,
             int len2,
             unsigned char *in,
             int *len,
             int skip );

#ifdef WBUS_HOST
/* WBUS host support */
int wbus_host_listen( HANDLE_WBUS wbus,
                      unsigned char *addr,
                      unsigned char *cmd,
                      unsigned char *data,
                      int *len);

int wbus_host_answer( HANDLE_WBUS wbus, 
                      unsigned char addr,
                      unsigned char cmd,
                      unsigned char *data,
                      int len);
#endif

/* Hight Level I/O */
int wbus_sensor_read(HANDLE_WBUS wbus, HANDLE_WBSENSOR s, int idx);
void wbus_sensor_print(char *str, HANDLE_WBSENSOR s);

int wbus_get_wbinfo(HANDLE_WBUS wbus, HANDLE_WBINFO hInfo);
void wbus_ident_print(char *str, HANDLE_WBINFO i, int line);

/* Error code handling */
int wbus_errorcodes_read(HANDLE_WBUS wbus, HANDLE_WBERR e);
int wbus_errorcodes_clear(HANDLE_WBUS wbus);
void wbus_errorcode_print(char *str, HANDLE_WBERR e, int i);

/* Test heater subsystems */
int wbus_test(HANDLE_WBUS wbus, unsigned char test, unsigned char secds, float value);

/* Turn heater on/off */
typedef enum {
  WBUS_VENT,
  WBUS_SH,
  WBUS_PH
} WBUS_TURNON;

int wbus_turnOn(HANDLE_WBUS wbus, WBUS_TURNON mode, unsigned char time);
int wbus_turnOff(HANDLE_WBUS wbus);
/* Check or keep alive heating process. mode is the argument that
   was passed as to wbus_turnOn() */
int wbus_check(HANDLE_WBUS wbus, WBUS_TURNON mode);

/*
 * Fuel Priming
 */
int wbus_fuelPrime(HANDLE_WBUS wbus, unsigned char time);

int wbus_eeprom_read(HANDLE_WBUS wbus, int addr, unsigned char *eeprom_data);
int wbus_eeprom_write(HANDLE_WBUS wbus, int addr, unsigned char *eeprom_data);

int wbus_data_set_load(HANDLE_WBUS wbus, void *seq, unsigned char idx);
int wbus_data_set_store(HANDLE_WBUS wbus, void *seq, unsigned char idx);

#endif /* __WBUS_H__ */
