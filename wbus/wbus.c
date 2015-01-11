/*
 * W-Bus access interface
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#include "wbus.h"
#include "rs232.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wbus_const.h"
#include "machine.h"

#define WBMSGLEN_MAX 32

struct WBUS
{
  HANDLE_RS232 rs232;
};

/*
 * \param buf pointer to ACK message part
 * \param len length of data
 * \param chk initial value of the checksum. Useful for concatenating.
 */
static unsigned char checksum(unsigned char *buf, unsigned char len, unsigned char chk)
{	 
  for (;len!=0; len--) {
    chk ^= *buf++;
  }
 
  return chk;
}

/**
 * Send request to heater and one or two consecutive buffers.
 * \param wbus wbus handle
 * \param cmd wbus command to be sent
 * \param data pointer for first buffer.
 * \param len length of first data buffer.
 * \param data pointer to an additional buffer.
 * \param len length the second data buffer.
 * \return 0 if success, 1 on error.
 */
static int wbus_msg_send( HANDLE_WBUS wbus,
                          unsigned char addr,
                          unsigned char cmd,
                          unsigned char *data,
                          int len,
                          unsigned char *data2,
                          int len2)
{
  unsigned char bytes, chksum;
  unsigned char buf[3];
	 
  /* Assemble packet header */
  buf[0] = addr;
  buf[1] = len + len2 + 2;
  buf[2] = cmd;

  chksum = checksum(buf, 3, 0);
  chksum = checksum(data, len, chksum);
  chksum = checksum(data2, len2, chksum);

  /* Send message */
  rs232_flush(wbus->rs232);
  rs232_write(wbus->rs232, buf, 3);
  rs232_write(wbus->rs232, data, len);
  rs232_write(wbus->rs232, data2, len2);
  rs232_write(wbus->rs232, &chksum, 1);

  /* Read and check echoed header */
  bytes = rs232_read(wbus->rs232, buf, 3);
  if (bytes != 3) {
    PRINTF("wbus_msg_send() K-Line error. %d < 3\n", bytes);
    return -1;
  }
  if (buf[0] != addr || buf[1] != (len+len2+2)  || buf[2] != cmd) {
    PRINTF("wbus_msg_send() K-Line error %x %x %x\n", buf[0], buf[1], buf[2]);
    return -1;
  }
	 
  /* Read and check echoed data */
  {
    int i;

    for (i=0; i<len; i++) {
      bytes = rs232_read(wbus->rs232, buf, 1);
      if (bytes != 1 || buf[0] != data[i]) {
        PRINTF("wbus_msg_send() K-Line error. %d < 1 (data2)\n", bytes);
        return -1;
      }
    }
    for (i=0; i<len2; i++) {
      bytes = rs232_read(wbus->rs232, buf, 1);
      if (bytes != 1 || buf[0] != data2[i]) {
        PRINTF("wbus_msg_send() K-Line error. %d < 1 (data2)\n", bytes);
        return -1;
      }
    }
  }

  /* Check echoed checksum */
  bytes = rs232_read(wbus->rs232, buf, 1);
  if (bytes != 1 || buf[0] != chksum) {
    PRINTF("wbus_msg_send() K-Line error. %d < 1 (data2)\n", bytes);
    return -1;
  }
                        
  return 0;
}

/*
 * Read answer from wbus
 * addr: source/destination address pair to be expected or returned in case of host I/O
 * cmd: if pointed location is zero, received client message, if not send client message
        with command *cmd.
 * data: buffer to either receive client data, or sent its content if client (*cmd!=0).
         If *data is !=0, then *data amount of data bytes will  be skipped.
 * dlen: out: length of data.
 */
static int wbus_msg_recv( HANDLE_WBUS wbus,
                          unsigned char *addr,
                          unsigned char *cmd,
                          unsigned char *data,
                          int *dlen,
                          int skip)
{
  unsigned char buf[3];
  unsigned char chksum;
  int len;
  
  /* Read address header */
  do {
    if (rs232_read(wbus->rs232, buf, 1) != 1) {
#ifdef HAS_PRINTF
      if (*cmd != 0) {
        PRINTF("wbus_msg_recv(): timeout\n");
      }
#endif
      return -1;
    }

    buf[1] = buf[0];
#ifdef WBUS_HOST
    if (*cmd == 0) {
      buf[1] &= 0x0f;
    }
#endif
    /* Check addresses */
  } while ( buf[1] != *addr );

  rs232_blocking(wbus->rs232, 0);

  /* Read length and command */
  if (rs232_read(wbus->rs232, buf+1, 2) != 2) {
#ifdef HAS_PRINTF
    if (*cmd != 0) {
      PRINTF("wbus_msg_recv(): No addr/len error\n");
    }
#endif
    return -1;
  }

#ifdef WBUS_HOST
  if (*cmd != 0)
#endif
  {
    /* client case: check ACK */  
    if (buf[2] != (*cmd|0x80)) {
      PRINTF("wbus_msg_recv() Request %x was rejected\n", *cmd);
      /* Message reject happens. Do not be too picky about that. */
      *dlen = 0;
      return 0;
    }
  }
#ifdef WBUS_HOST
  else {
    *addr = buf[0];
    *cmd = buf[2];
  }
#endif
	
  /* PRINTF("received: %x %x %x \n", buf[0], buf[1], buf[2] ); */

  chksum = checksum(buf, 3, 0);

  /* Read possible data */
  len = buf[1] - 2 - skip;
  if (len > 0 || skip > 0)
  {
    for (;skip>0; skip--) {
      if (rs232_read(wbus->rs232, buf, 1) != 1) {
        PRINTF("wbus_msg_recv() Read error on data skip\n");
        return -1;
      }
      chksum = checksum(buf, 1, chksum);
    }
    if (len > 0) {
      if (rs232_read(wbus->rs232, data, len) != len) {
        PRINTF("wbus_msg_recv() Read error. len=%d skip=%d cmd=0x%x buf[1]=0x%x\n", len, skip, *cmd, buf[1]);
        return -1;
      }
      chksum = checksum(data, len, chksum);
    }
    /* Store data length */
    *dlen = len;
  } else {
    *dlen = 0;
  }
  
  /* Read and verify checksum */
  rs232_read(wbus->rs232, buf, 1);
  /*
  if (*buf != chksum) {
    PRINTF("wbus_msg_recv() Checksum error\n");
    return -1;
  }
  */
  
  return 0;
}

void wbus_init(HANDLE_WBUS wbus)
{
  /* Initialization sequence */
  rs232_sbrk(wbus->rs232, 1);
  machine_usleep(50000);
  rs232_sbrk(wbus->rs232, 0);
  machine_usleep(50000);
  /* Empty all queues. BRK toggling may cause a false received byte (or more than one who knows). */
  rs232_flush(wbus->rs232);
}

/*
 * Send a client W-Bus request and read answer from Heater.
 */
int wbus_io( HANDLE_WBUS wbus,
             unsigned char cmd,
             unsigned char *out,
             unsigned char *out2,
             int len2,
             unsigned char *in,
             int *dlen,
             int skip)
{
  int err, tries;
  int len;
  unsigned char addr;
  
  len = *dlen;

  rs232_blocking(wbus->rs232, 0);

  tries = 0;
  do {
    if (tries != 0) {
      PRINTF("wbus_io() retry: %d\n", tries);
      machine_msleep(500);
      wbus_init(wbus);
    }
    
    tries++;
    /* Send Message */
    addr = (WBUS_CADDR<<4) | WBUS_HADDR;
    err = wbus_msg_send(wbus, addr, cmd, out, len, out2, len2);
    if (err != 0) {
      PRINTF("wbus_msg_send returned %d\n", err);
      continue;
    }
     
    /* Receive reply */
    addr = (WBUS_HADDR<<4) | WBUS_CADDR;
    err = wbus_msg_recv(wbus, &addr, &cmd, in, dlen, skip);
    if (err != 0) {
      PRINTF("wbus_msg_recv returned %d\n", err);
      continue;
    }
    
  } while (tries < 4 && err != 0);

  return err;
}

#ifdef WBUS_HOST
/*
 * Listen to client W-Bus requests.
 */
int wbus_host_listen( HANDLE_WBUS wbus,
                      unsigned char *addr,
                      unsigned char *cmd,
                      unsigned char *data,
                      int *len)
{
  int err;
  
  do {
    *cmd = 0;
    *addr = WBUS_HADDR;
    rs232_blocking(wbus->rs232, 1);
    err = wbus_msg_recv(wbus, addr, cmd, data, len, 0);
  } while (err);

    
  return err;
}

/*
 * Send answer to W-Bus client.
 */
int wbus_host_answer( HANDLE_WBUS wbus, 
                      unsigned char addr,
                      unsigned char cmd,
                      unsigned char *data,
                      int len)
{
  addr = (WBUS_HADDR<<4) | (addr>>4); 
  //PRINTF("sending %x %x %x %x %x ... \n", addr, len+3, cmd, data[0], data[1] );
  return wbus_msg_send(wbus, addr, cmd|0x80, data, len, NULL, 0);
}
#endif /* WBUS_HOST */

/* Overall info*/
int wbus_get_wbinfo(HANDLE_WBUS wbus, HANDLE_WBINFO i)
{
  int err, len;
  unsigned char tmp, tmp2[2];
  
  wbus_init(wbus);

  tmp = IDENT_WB_VER; len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, &i->wbus_ver, &len, 1);
  if (err) goto bail;
  
  tmp = IDENT_DEV_NAME; len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, (unsigned char*)i->dev_name, &len, 1);
  if (err) goto bail;
  i->dev_name[len] = 0; /* Hack: Null terminate this string */
#if 0
  tmp = 3;              len = 1; err = wbus_io(wbus, WBUS_CMD_SL_RD, &tmp, NULL, 0, i->strange2, &len, 1);
#else
  tmp2[0]=3; tmp2[1]=7; len = 2; err = wbus_io(wbus, WBUS_CMD_SL_RD, tmp2, NULL, 0, i->strange2, &len, 2);
#endif
  tmp = IDENT_WB_CODE;  len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->wbus_code, &len, 1);
                        len = 0; err = wbus_io(wbus, WBUS_CMD_U1,  NULL, NULL, 0, i->strange, &len, 0);

  if (err) goto bail;
  tmp = IDENT_DEV_ID;  len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->dev_id, &len, 1);
  if (err) goto bail;
  tmp = IDENT_HWSW_VER;len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->hw_ver, &len, 1);
  if (err) goto bail;
  tmp = IDENT_DATA_SET;len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->data_set_id, &len, 1);
  if (err) goto bail;
  tmp = IDENT_DOM_CU;  len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->dom_cu, &len, 1);
  if (err) goto bail;
  tmp = IDENT_DOM_HT;  len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->dom_ht, &len, 1);
  if (err) goto bail;
  tmp = IDENT_U0;      len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->u0, &len, 1);
  if (err) goto bail;
  tmp = IDENT_CUSTID;  len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->customer_id, &len, 1);
  if (err) goto bail;
  tmp = IDENT_SERIAL;  len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->serial, &len, 1);
  if (err) goto bail;
  tmp = IDENT_SW_ID;   len = 1; err = wbus_io(wbus, WBUS_CMD_IDENT, &tmp, NULL, 0, i->sw_id, &len, 1);

bail:
  return err;
}

static char* hexdump(char *str, unsigned char *d, int l)
{
  for (l--;l!=-1;l--)
    str += sprintf(str, "%02x", *d++);
  /* str += sprintf(str, "\n"); */
	
  return str;
}

static const char *_wd[6] = { "StrangeDay", "Monday ", "Tuesday ", "Wednesday ", "Thursday ", "Friday " };

static char* getVersion(char *str, unsigned char *d)
{
  strcpy(str, _wd[d[0]]);
  str += strlen(str);
  str += sprintf(str, "%x/%x  %x.%x", d[1], d[2], d[3], d[4]);

  return str;
}

void wbus_ident_print(char *str, HANDLE_WBINFO i, int line)
{
  switch (line) {
    case 0: str += sprintf(str, "W-Bus version: %d.%d", (i->wbus_ver>>4)&0x0f, (i->wbus_ver&0x0f)); break;
    case 1: str += sprintf(str, "Device Name: %s", i->dev_name); break;
    case 2: str += sprintf(str, "W-Bus code: "); str=hexdump(str, i->wbus_code, 7); break;
    case 3: str += sprintf(str, "Device ID Number: "); str=hexdump(str, i->dev_id, 5); break;
    case 4: str += sprintf(str, "Data set ID Number: "); str=hexdump(str, i->data_set_id, 6); break;
    case 5: str += sprintf(str, "Software ID Number: "); str=hexdump(str, i->sw_id, 5); break;
    case 6: str += sprintf(str, "Hardware version: %x/%x", i->hw_ver[0], i->hw_ver[1] ); break;
    case 7: str += sprintf(str, "Software version: "); str=getVersion(str, i->sw_ver); break;
    case 8: str += sprintf(str, "Software version EEPROM: "); str=getVersion(str, i->sw_ver_eeprom); break;
    case 9: str += sprintf(str, "Date of Manufacture Control Unit: %x.%x.%x", i->dom_cu[0], i->dom_cu[1], i->dom_cu[2] ); break;
    case 10: str += sprintf(str, "Date of Manufacture Heater: %x.%x.%x", i->dom_ht[0], i->dom_ht[1], i->dom_ht[2] ); break;
    case 11: str += sprintf(str, "Customer ID Number: %s", i->customer_id ); break;
    case 12: str += sprintf(str, "Serial Number: "); str=hexdump(str, i->serial, 5); break;
    case 13: str += sprintf(str, "Test signature: "); str=hexdump(str, i->test_signature, 2); break;
    case 14: str += sprintf(str, "Sensor8: "); str=hexdump(str, i->u0, 7); break;
    case 15: str += sprintf(str, "U1: "); str=hexdump(str, i->strange, 7); break;
    case 16: str += sprintf(str, "System Level: "); str=hexdump(str, i->strange2, 1); break;
  }
}

/* Sensor access */
int wbus_sensor_read(HANDLE_WBUS wbus, HANDLE_WBSENSOR sensor, int idx)
{
  int err  = 0;
  int len;
  unsigned char sen;
	
  /* Tweak: skip some addresses since reading them just causes long delays. */
  switch (idx) {
    default:
    break;
    case 0:
    case 1:
    case 8:
    case 9:
    case 13:
    case 14:
    case 16:
      sensor->length = 0;
      sensor->idx = 0xff;
      return -1;
      break;	
  }
	
  wbus_init(wbus);
	
  sen = idx; len=1;
  err = wbus_io(wbus, WBUS_CMD_QUERY, &sen, NULL, 0, sensor->value, &len, 1);
  if (err != 0)
  {
    PRINTF("Reading sensor %d failed\n", idx);
    sensor->length = 0;
    return -1;
  }
  
  /* Store length of received value */
  sensor->length = len;
  sensor->idx=idx;	
  return err;
}

#define BOOL(x) (((x)!=0)?1:0)

void wbus_sensor_print(char *str, HANDLE_WBSENSOR s)
{	
  unsigned char v;

  if (s->idx == 0xff)
  {
    strcpy(str, "skipped");
    return; 
  }
  
  switch (s->idx) {
     case QUERY_STATUS0:
       str += sprintf(str, "SHR: %d, MS: %d, S: %d, D: %d, BOOST: %d AD: %d, T15: %d",
         s->value[0]&STA00_SHR, s->value[0]&STA00_MS,
         s->value[0]&STA01_S, s->value[0]&STA02_D,
         s->value[0]&STA03_BOOST, s->value[0]&STA03_AD,
         s->value[0]&STA04_T15);
       break;
     case QUERY_STATUS1:
        v=s->value[0]; str += sprintf(str, "State CF=%d GP=%d CP=%d VFR=%d", 
		BOOL(v&STA10_CF), BOOL(v&STA10_GP), BOOL(v&STA10_CP), BOOL(v&STA10_VF));
        break;
     case QUERY_OPINFO0:
       str += sprintf(str, "Fuel type %x, Max heating time %d [min], factors %d %d",
         s->value[0], s->value[1]*10, s->value[2], s->value[3]);
       break;
     case QUERY_SENSORS:
	str += sprintf(str, "Temp = %d, Volt = ", BYTE2TEMP(s->value[0]));
        str += WORD2VOLT_TEXT(str, s->value+1);
        str += sprintf(str, ", FD = %d, HE = %d, GPR = %d", 
		s->value[3], WORD2WATT(s->value+4), s->value[6]);
        str += BYTE2GPR_TEXT(str, s->value[7]);
        break;
     case QUERY_COUNTERS1:
        str += sprintf(str, "Working hours %d:%d  Operating hours %u:%u Start Count %u",
          WORD2HOUR(s->value), s->value[2], WORD2HOUR(s->value+3), s->value[5], twobyte2word(s->value+6));
        break;
     case QUERY_STATE:
       str += sprintf(str, "OP state: 0x%x, N: %d, Dev state: 0x%x", s->value[0], s->value[1], s->value[2]);
       break;
     case QUERY_DURATIONS0:
        /* 
           PH=parking heating, SH= supplemental heatig.
           format: hours:minutes, 1..33%,34..66%,67..100%,>100%
         */
        str += sprintf(str, "Burning duration PH %u:%u %u:%u %u:%u %u:%u SH %u:%u %u:%u %u:%u %u:%u",
          WORD2HOUR(s->value), s->value[2], WORD2HOUR(s->value+3), s->value[5], WORD2HOUR(s->value+6), s->value[8], 
          WORD2HOUR(s->value+9), s->value[11],
          WORD2HOUR(s->value+12), s->value[14], WORD2HOUR(s->value+15), s->value[17], WORD2HOUR(s->value+18), s->value[20],
          WORD2HOUR(s->value+21), s->value[23]);
       break;
     case QUERY_DURATIONS1:
        str += sprintf(str, "Working duration PH %u:%u SH %u:%u",
          WORD2HOUR(s->value), s->value[2], WORD2HOUR(s->value+3), s->value[5]);
       break; 
     case QUERY_COUNTERS2:
        str += sprintf(str, "Start Counter PH %d SH %d other %d",
          twobyte2word(s->value), twobyte2word(s->value+2), twobyte2word(s->value+4));
       break;
     case QUERY_STATUS2:
        /* Level in percent. Too bad the % sign cant be printed reasonably on an 14 segment LCD. */
        str += sprintf(str, "Level GP:%d FP:%d Hz CF:%d %02x CP: %02x",
           s->value[0]>>1, s->value[1]/20, s->value[2]>>1,
           s->value[3], s->value[4]>>1);
        break;
     case QUERY_OPINFO1:\
       str += sprintf(str, "Low temp thres %d, High temp thres %d, Unknown 0x%x",
          BYTE2TEMP(s->value[OP1_TLO]),
          BYTE2TEMP(s->value[OP1_THI]),
          s->value[OP1_U0]);
       break;
     case QUERY_DURATIONS2:
        str += sprintf(str, "Ventilation duration %d:%d",
          WORD2HOUR(s->value), s->value[2]);
        break;
     case QUERY_FPW:
	str += sprintf(str, "FPW = ");
#if 0
	str += WORD2FPWR_TEXT(str, s->value);
	str += sprintf(str, "Ohm, %d Watt", WORD2WATT(s->value+2)); 
#else
        str += sprintf(str, "%d 'C, %d Watt", WORD2WATT(s->value), WORD2WATT(s->value+2)); 
#endif
        break;
     default:
	str += sprintf(str, "Sensor %d, value = ", s->idx); str=hexdump(str, s->value, s->length);
	break;
  }	
}

/* error code handling */
int wbus_errorcodes_read(HANDLE_WBUS wbus, HANDLE_WBERR e)
{
  int err  = 0, i, nErr, len;
  unsigned char list[32];
  unsigned char tmp[2];
	
  if (wbus == NULL)
  {
    PRINTF("wbus_errorcodes_read() Invalid handle\n");
    return -1;
  }

  wbus_init(wbus); 
	
  tmp[0] = ERR_LIST;
  len = 1;
  err = wbus_io(wbus, WBUS_CMD_ERR, tmp, NULL, 0, list, &len, 1);
  
  nErr = list[0];
  PRINTF("found %d errors\n", nErr);

  /* Extract info from list and read data of each error */
  for (i=0; i<nErr; i++)
  {
    len = 2;
    tmp[0] = ERR_READ;
    tmp[1] = list[i*2+1];
    err = wbus_io(wbus, WBUS_CMD_ERR, tmp, NULL, 0, (unsigned char*)&e->errors[i].info, &len, 1);
  }
  e->nErr = nErr;
	 
  return err;
}

int wbus_errorcodes_clear(HANDLE_WBUS wbus)
{
  int err, len;
  unsigned char tmp;
	 
  wbus_init(wbus); 
	
  tmp = ERR_DEL; len = 1; err = wbus_io(wbus, WBUS_CMD_ERR, &tmp, NULL, 0, &tmp, &len, 0);
	 
  return err;
}

void wbus_errorcode_print(char *str, HANDLE_WBERR e, int i)
{
  /* Print info about given error index */
  if (e->nErr == 0)
  {
    strcpy(str, "No Errors :)");
  }
  else
  {
    err_info_t *info = &e->errors[i].info;
		  
    str += sprintf(str, "Code 0x%x", info->code);
    str += sprintf(str, " Flags %x", info->flags);
    str += sprintf(str, " Counter %d", info->counter);
    str += sprintf(str, " OP State %d, %d", info->op_state[0], info->op_state[1] );
    str += sprintf(str, " Temp %d C Supply ", BYTE2TEMP(info->temp));
    str += WORD2VOLT_TEXT(str, info->volt);
    str += sprintf(str, " Volt OP time %d:%d", WORD2HOUR(info->hour), info->minute); 
  }
}


/* Test */
int wbus_test(HANDLE_WBUS wbus, unsigned char test, unsigned char secds, float value)
{
  int err = 0;
  unsigned char tmp[4];
  int len = 4;

  wbus_init(wbus);
  	
  tmp[0] = test;
  tmp[1] = secds;
	
  if (test == WBUS_TEST_FP)
    FREQ2WORD(tmp[2], value);
  else
    PERCENT2WORD(tmp[2], value);
	
  err = wbus_io(wbus, WBUS_CMD_TEST, tmp, NULL, 0, tmp, &len, 0);
	
  return err;
}

/* Turn heater on for time minutes */
int wbus_turnOn(HANDLE_WBUS wbus, WBUS_TURNON mode, unsigned char  time)
{
  int err = 0;
  unsigned char cmd;
  unsigned char tmp[1];
  int len = 1; 

  switch (mode) {
    case WBUS_VENT: cmd = WBUS_CMD_ON_VENT; break;
    case WBUS_SH: cmd = WBUS_CMD_ON_SH; break;
    case WBUS_PH: cmd = WBUS_CMD_ON_PH; break;
    default:
       return -1;
  }
  
  wbus_init(wbus); 
  
  tmp[0] = time;
  err = wbus_io(wbus, cmd, tmp, NULL, 0, tmp, &len, 0);
	  
  return err;
}

/* Turn heater off */
int wbus_turnOff(HANDLE_WBUS wbus)
{
  int err = 0;
  int len = 0;
  unsigned char tmp[2];
	
  wbus_init(wbus); 
	
  err = wbus_io(wbus, WBUS_CMD_OFF, tmp, NULL, 0, tmp, &len, 0);
	  
  return err;
}

/* Check current command */
int wbus_check(HANDLE_WBUS wbus, WBUS_TURNON mode)
{
  int err = 0;
  int len = 2;
  unsigned char tmp[3];

  switch (mode) {
    case WBUS_VENT: tmp[0] = WBUS_CMD_ON_VENT; break;
    case WBUS_SH: tmp[0] = WBUS_CMD_ON_SH; break;
    case WBUS_PH: tmp[0] = WBUS_CMD_ON_PH; break;
    default:
       return -1;
  }
  tmp[1] = 0;
  
  wbus_init(wbus); 
  
  err = wbus_io(wbus, WBUS_CMD_CHK, tmp, NULL, 0, tmp, &len, 0);
	  
  return err;
}

/*
 * Fuel Priming
 */
int wbus_fuelPrime(HANDLE_WBUS wbus, unsigned char time)
{
  unsigned char tmp[5];
  int len;
  
  wbus_init(wbus); 
  
  tmp[0] = 0x03;
  tmp[1] = 0x00;
  tmp[2] = time>>1;
  len = 3;
  
  return wbus_io(wbus, WBUS_CMD_X, tmp, NULL, 0, tmp, &len, 0);
}

int wbus_eeprom_read(HANDLE_WBUS wbus, int addr, unsigned char *eeprom_data)
{
  unsigned char tmp[1];
  int len;
  
  wbus_init(wbus); 
  
  tmp[0] = addr;
  len = 1;
  
  return wbus_io(wbus, WBUS_TS_ERD, tmp, NULL, 0, eeprom_data, &len, 2);
}

int wbus_eeprom_write(HANDLE_WBUS wbus, int addr, unsigned char *eeprom_data)
{
  unsigned char tmp[3];
  int len;
  
  wbus_init(wbus); 
  
  tmp[0] = addr;
  len = 1;
  
  return wbus_io(wbus, WBUS_TS_EWR, tmp, eeprom_data, 1, tmp, &len, 0);
}


/*
 * Library Self test
 */
#ifdef WBUS_TEST


#include <string.h>
/*
 * Static tests
 */
void wbus_teststatic(void)
{
  const unsigned char msg1[] = {0xf4, 0x04, 0x56, 0x02, 0x98, 0x3c};
  const unsigned char msg2[] = {0x4f, 0x0e, 0xd6, 0x02, 0x98, 0x00, 0x01, 0x04,
		  0x00, 0x46, 0x35, 0x7a, 0x03, 0x97, 0x1b, 0x8e};
	 
  PRINTF("Testing checksum\n");
  if (checksum(msg1, 5, 0) != 0x3c)
    PRINTF("Checksum on msg1 failed! chksum = %x\n", checksum(msg1, 5, 0));
  if (checksum(msg2, 15, 0) != 0x8e)
    PRINTF("Checksum on msg2 failed! chksum = %x\n", checksum(msg2, 15, 0));

}
#endif

int wbus_data_set_load(HANDLE_WBUS wbus, void *seq, unsigned char idx)
{
  int err = 0;
  int len = 2;
  unsigned char *tmp = seq;

  tmp[0] = DATASET_READ;
  tmp[1] = idx;

  wbus_init(wbus);
  
  err = wbus_io(wbus, WBUS_CMD_DATASET, tmp, NULL, 0, seq, &len, 2);

  return err;

}

int wbus_data_set_store(HANDLE_WBUS wbus, void *seq, unsigned char idx)
{
  int err = 0;
  int len = 2;
  unsigned char tmp[2];

  tmp[0] = DATASET_WRITE;
  tmp[1] = idx;

  wbus_init(wbus);
  
  err = wbus_io(wbus, WBUS_CMD_DATASET, tmp, seq, 96, NULL, &len, 96+2);

  return err;
}

/* Overall handling stuff */

#ifndef WBUS_USE_MALLOC
static struct WBUS _wbus[NO_RS232];
#endif
 
int wbus_open(HANDLE_WBUS *pWbus, unsigned char dev_idx)
{
  HANDLE_WBUS wbus;
  int err;

#ifdef WBUS_USE_MALLOC
  wbus = malloc(sizeof(struct WBUS));
#else
  wbus = &_wbus[dev_idx];
#endif
  err = rs232_open(&wbus->rs232, dev_idx, 2400, RS232_FMT_8E1);
    
  if (err == 0)
    *pWbus = wbus;
    	 
#ifdef WBUS_TEST
  wbus_teststatic();
#endif
	 
  return err;
}

void wbus_close(HANDLE_WBUS wbus)
{
  if (wbus == NULL)
    return;
	 
  rs232_close(wbus->rs232);
#ifdef WBUS_USE_MALLOC
  free(wbus);
#endif
}
