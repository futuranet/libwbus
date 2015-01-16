/*
 * GSM remote control
 */

#include "gsmctl.h"
#include "rs232.h"
#include "machine.h"
#include "kernel.h"
#include <string.h>
#include <stdio.h>

#ifdef __MSP430_449__
#define GSM_BAUDRATE 19200
#elif defined __MSP430_149__
/* As recommended here: http://e2e.ti.com/support/microcontrollers/msp430/f/166/t/90913.aspx */
#define GSM_BAUDRATE 4800
#define GSMCTL_TELIT
#else
#define GSM_BAUDRATE 115200
#endif

#define GSMCTL_ADDNUMBER   1 /* The next +CLIP message is registered as an allowed number */
#define GSMCTL_GOTNUMBER   2 /* A phone number was captured after GSMCTL_ADDNUMBER was set */
#define GSMCTL_POWERON     4 /* GSM modem is assumed to be powered on upon AT command test */
#define GSMCTL_REGISTERED  8 /* GSM modem reported to be registered to a network */
#define GSMCTL_ROAMING    16 /* GSM modem uses roaming */
#define GSMCTL_DENIED     32 /* GSM modem network registration was denied */

struct gsmctl{
  HANDLE_RS232 rs232;
  int no_io_timeout;
  int reset_timeout;
  gsmcallback cb;
  void *cb_data;
  char (*allowedNumbers)[32];
  unsigned char flags;
  unsigned char gotIdx;
  /* Signal quality indices */
  unsigned char rssi;
  unsigned char ber;
};

static char tmp[32+1];

/*
 * Send AT command and get result.
 */
static int gsmctl_atio(HANDLE_GSMCTL hgsmctl, char *cmd, char *result)
{
  int bytes, tmp;

  bytes = strlen(cmd);
  rs232_flush(hgsmctl->rs232);  
  rs232_write(hgsmctl->rs232, (unsigned char*)cmd, bytes);
  /* Skip command echo */
  kernel_sleep(MSEC2JIFFIES(500));
  kernel_yield();
  tmp = rs232_read(hgsmctl->rs232, (unsigned char*)result, bytes);
  /* Give AT command enough time. */
  for (tmp=0; tmp<50; tmp++) {
    kernel_sleep(MSEC2JIFFIES(100));
    kernel_yield();
    if (rs232_rxBytes(hgsmctl->rs232) > 2) {
      break;
    }
  }
  /* Read result data */
  for (bytes=0; bytes<32; bytes++)
  {
    tmp = rs232_read(hgsmctl->rs232, (unsigned char*)result+bytes, 1);
    /* break if no data */
    if (tmp != 1) {
      bytes++;
      break;
    }
    if (bytes > 0) {
      /* Break if OK returned */
      if (result[bytes-1] == 'O' && result[bytes] == 'K') {
        tmp = 2;
        bytes++;
        break;
      }
    }
  }
  /* Null terminate result */
  result[bytes] = 0;

  if (tmp != 2) {
    bytes = -1;
  }

  return bytes;
}

/*
 * Check GSM modem status.
 */
static
int gsmctl_init(HANDLE_GSMCTL hgsmctl)
{
  int bytes;

#ifdef GSMCTL_TELIT
  if ( (hgsmctl->flags & GSMCTL_POWERON) == 0 )
  {
    /* Init Telit GSM modem */
    if ((P6OUT & BIT2) == 0) {
      P6OUT |= BIT2; /* Enable L5973 */
      kernel_sleep(MSEC2JIFFIES(1000));
      kernel_yield();
      bytes = -1; /* Because L5973 was off, force GSM_ON_OFF toggle */
    } else {
      /* Test if modem is already on to skip GSM_ON_OFF toggle. */
      bytes = gsmctl_atio(hgsmctl, "atz\r", tmp);
    }
    if (bytes < 0)
    {
      P6OUT |= BIT4; /* Toggle GSM_ON_OFF */
      kernel_sleep(MSEC2JIFFIES(1000));
      kernel_yield();
      P6OUT &= ~BIT4;
      /* Wait until modem power up */
      kernel_sleep(MSEC2JIFFIES(1000));
      kernel_yield();
    }
  }
#endif
 
  bytes = gsmctl_atio(hgsmctl, "atz\r", tmp);
  if (bytes < 0) {
    goto bail;
  }  
  bytes = gsmctl_atio(hgsmctl, "at+clip=1\r", tmp);
  if (bytes < 0) {
    goto bail;
  }
  hgsmctl->flags |= GSMCTL_POWERON;

#ifdef GSMCTL_TELIT
  bytes = gsmctl_atio(hgsmctl, "at#nitz=1,0\r", tmp);
  if (bytes < 0) {
    goto bail;
  }
#endif

  /* Check GSM network registration */
  bytes = gsmctl_atio(hgsmctl, "at+creg?\r", tmp);
  if (bytes < 0) {
    hgsmctl->flags &= ~(GSMCTL_REGISTERED|GSMCTL_ROAMING);
  } else {
    char *ptr;
    /* Check status to be either 1 (home network) or 5 (roaming) */
    /* \r\n+CREG: 0,1\r\n\r\nOK\r\n */
    ptr = strstr(tmp, "0,");
    if (ptr != NULL)
    {
      switch (ptr[2]) {
      case '5':
        hgsmctl->flags |= GSMCTL_ROAMING;
      case '1': 
        hgsmctl->flags |= GSMCTL_REGISTERED;
        hgsmctl->flags &= ~GSMCTL_DENIED;
        break;
      case '3':
        hgsmctl->flags |= GSMCTL_DENIED;
      default:
        hgsmctl->flags &= ~(GSMCTL_REGISTERED|GSMCTL_ROAMING);
        break;
      }
    } else {
      hgsmctl->flags &= ~(GSMCTL_REGISTERED|GSMCTL_ROAMING|GSMCTL_DENIED);
    }
  }

  if ((hgsmctl->flags & GSMCTL_REGISTERED) == 0) {
    return -2;
  }

  /* Get some info about link quality. This should tell you if the antenna is OK. */
  if (hgsmctl->flags & GSMCTL_REGISTERED)
  {
    gsmctl_atio(hgsmctl, "at+csq\r", tmp);
    {
      char *ptr = tmp;
      
      /* \r\n+CSQ: 22,0\r\n\r\nOK\0 */
      while (ptr < (tmp+32) && *ptr != ',') {
        ptr++;
      }
      if (*ptr == ',')
      {
        bytes = ptr[-1] - '0';
        if (ptr[-2] > '0' && ptr[-2] <= '9') {
          bytes += 10*(ptr[-2]-'0');
        }
        hgsmctl->rssi = bytes;

        bytes = ptr[1] - '0';
        if (ptr[2] >= '0' && ptr[2] <= '9') {
          bytes = bytes*10 + (ptr[2] - '0');
        }
        hgsmctl->ber = bytes;
      }
    }
  }

  return 0;

bail:
  hgsmctl->flags &= ~(GSMCTL_POWERON|GSMCTL_REGISTERED|GSMCTL_ROAMING|GSMCTL_DENIED);

  return -1;
}

int gsmctl_getTime(HANDLE_GSMCTL hgsmctl, rtc_time_t *time)
{
  char *ptr;

  if (hgsmctl->flags & GSMCTL_ADDNUMBER) {
    return -1;
  }

  /* Ignore return value because cclk? result does not contain OK */
  gsmctl_atio(hgsmctl, "at+cclk?\r", tmp);

  /* yy/MM/dd,hh:mm:ss±zz */
  /* \r\n+CCLK: \"14/10/12,21:08:40\"\r\n\r\n */
  ptr = strstr(tmp, "\"");
  if (ptr == NULL) {
    return -1;
  }
  if (ptr[3] != '/' && ptr[6] != '/' && ptr[9] != ',' ) {
    return -1;
  }
  time->seconds = (ptr[16]-'0')*10 + (ptr[17]-'0');
  time->minutes = (ptr[13]-'0')*10 + (ptr[14]-'0');
  time->hours = (ptr[10]-'0')*10 + (ptr[11]-'0');
#ifdef USE_CALENDAR
  time->day = (ptr[7]-'0')*10 + (ptr[8]-'0');
  time->month = (ptr[4]-'0')*10 + (ptr[5]-'0');
  time->year = (ptr[1]-'0')*10 + (ptr[2]-'0');
#endif
  return 0;
}

void gsmctl_getStatus(HANDLE_GSMCTL hgsmctl, char *stext)
{
  /* Note: this function does not do any serial I/O so that
  this function can be called from another thread too. */
  switch (hgsmctl->flags & (GSMCTL_POWERON|GSMCTL_REGISTERED|GSMCTL_ROAMING|GSMCTL_DENIED)) {
    case 0:
      strcpy(stext, "Off");
      break;
    case GSMCTL_POWERON:
      strcpy(stext, "Searching");
      break;
    case GSMCTL_POWERON|GSMCTL_REGISTERED:
      strcpy(stext, "Regd");
      break;
    case GSMCTL_POWERON|GSMCTL_REGISTERED|GSMCTL_ROAMING:
      strcpy(stext, "Roam");
      break;
    case GSMCTL_DENIED:
      strcpy(stext, "Denied");
      break;
  }

  if (hgsmctl->flags & GSMCTL_REGISTERED)
  {
    /* S  -65dB E 00 % */
    sprintf(stext, "S %4ddB E %02d %%", -113 + hgsmctl->rssi*2, hgsmctl->ber*2);
    /* apply 1/10 by shifting digits. */
    stext[13] = stext[12];
    stext[12] = '.';
  }  
}

/*
 * Add next incoming calling phone number to allowed list.
 */
void gsmctl_addNumber(HANDLE_GSMCTL hgsmctl)
{
  hgsmctl->no_io_timeout = 0; /* Stop lock out */
  hgsmctl->flags &= ~GSMCTL_GOTNUMBER;
  hgsmctl->flags |= GSMCTL_ADDNUMBER;
  hgsmctl->reset_timeout = 120; /* Give up after 2 minutes */
}

void gsmctl_addNumberCancel(HANDLE_GSMCTL hgsmctl)
{
  hgsmctl->flags &= ~(GSMCTL_ADDNUMBER|GSMCTL_GOTNUMBER);
}

/*
 * Get last number that was added to the allowed list.
 */
int gsmctl_getNewNumber(HANDLE_GSMCTL hgsmctl, char *out)
{
  if (hgsmctl->flags & GSMCTL_GOTNUMBER) {
    hgsmctl->flags &= ~GSMCTL_GOTNUMBER;
    strncpy(out, hgsmctl->allowedNumbers[hgsmctl->gotIdx], 32);
    return hgsmctl->gotIdx+1;
  }
  return 0;
}

/*  
 * Register a callback function for successful incoming requests (calls
 * initiated from a phone number that is allowed).
 */
void gsmctl_registerCallback(HANDLE_GSMCTL hgsmctl, gsmcallback cb, void *data)
{
  hgsmctl->cb = cb;
  hgsmctl->cb_data = data;
}

/*
 * Handle incoming calls
 */
void gsmctl_thread(HANDLE_GSMCTL hgsmctl)
{
  char *ptr;
  int x, y;

  /* Avoid multiple trigger of a single call. This is cheesy I know that... */
  if ( hgsmctl->no_io_timeout > 0 ) {
    hgsmctl->no_io_timeout--;
    rs232_flush(hgsmctl->rs232);
    return;
  }

  /* Do reset from time to time to ensure GSM modem is configured correctly */
  if ( hgsmctl->reset_timeout > 0 )
  {
    hgsmctl->reset_timeout--;
    if (hgsmctl->reset_timeout == 0)
    {
      if (hgsmctl->flags & GSMCTL_ADDNUMBER) {
        gsmctl_addNumberCancel(hgsmctl);
      } else {
        gsmctl_init(hgsmctl);
      }
      if (hgsmctl->flags & GSMCTL_REGISTERED) {
        hgsmctl->reset_timeout = 120;
      } else {
        hgsmctl->reset_timeout = 5;
      }
    }
  }
  
  /* look if there is an incomming call, if not bail */
  x = rs232_rxBytes(hgsmctl->rs232);
  if (x < 32)
    return;

  rs232_read(hgsmctl->rs232, (unsigned char*)tmp, 32);
  /* Set ptr to where the phone number starts, else leave */
  ptr = strstr(tmp, "+CLIP:");
  if (ptr == NULL)
    return;
  
  /* Skip over +CLIP: string */
  ptr += 6;
  x = (int)(ptr-tmp); /* x = position of phone number in tmp */
  /* Skip over any none digit characters. This skips also leading + or * */
  while ( (*ptr < '0' || *ptr > '9') && (x < 32) ) {
    ptr++;
    x = (int)(ptr-tmp); /* x = position of phone number in tmp */
  }

  /* In case of data garbage bail. */
  if ( x >= 32 ) {
    return;
  }

  y = 32 - x;             /* y = remaining valid data in tmp */
  memmove(tmp, ptr, y);   /* make room for more data if any. */
  rs232_read(hgsmctl->rs232, (unsigned char*)tmp + y, x);  /* get more text if any. */
  /* Find end of phone number */
  for (x=0; x<32; x++) {
    /* Find character that is not a digit */
    if ( tmp[x] < '0' || tmp[x] > '9' ) {
      tmp[x] = 0;
      break;
    }
  }
 
  /* If in add number mode, do so. */
  if (hgsmctl->flags & GSMCTL_ADDNUMBER)
  {
    hgsmctl->flags &= ~GSMCTL_ADDNUMBER;
    for (x=0; x<MAX_PHONE_NUMBERS; x++) {
      if (hgsmctl->allowedNumbers[x][0] == 0)
        break;
    }
    if (x < MAX_PHONE_NUMBERS) {
      strncpy(hgsmctl->allowedNumbers[x], tmp, 32);
      hgsmctl->flags |= GSMCTL_GOTNUMBER;
      hgsmctl->gotIdx = x; 
    }
    /* Lock out GSM I/O for 30 seconds */
    hgsmctl->no_io_timeout = 30;
    rs232_flush(hgsmctl->rs232);
  } else {
    /* Check if number is allowed */
    for (x=0; x<MAX_PHONE_NUMBERS; x++)
    {
      /* Check if entry in allowed list is valid */
      if (hgsmctl->allowedNumbers[x][0] != 0) {
        ptr = strstr(tmp, hgsmctl->allowedNumbers[x]);
        if (ptr != NULL) {
          /* Allowed number called */
          hgsmctl->cb(hgsmctl, hgsmctl->cb_data);
          rs232_flush(hgsmctl->rs232);
          /* Lock out GSM I/O for 30 seconds */
          hgsmctl->no_io_timeout = 30;
        }
      }
    }
  }
}

#ifndef USE_MALLOC
static struct gsmctl _gsmctl;
#endif

/*
 * 
 */
int gsmctl_open(HANDLE_GSMCTL *phgsmctl, GSM_NUMBERS fNumbers, int dev)
{
  HANDLE_GSMCTL hgsmctl = NULL;
  int err;
  
#ifdef USE_MALLOC
  hgsmctl = (HANDLE_GSMCTL)malloc(sizeof(struct gsmctl));
  if (hgsmctl == NULL)
    goto bail;
#else
  hgsmctl = &_gsmctl;
#endif

  hgsmctl->allowedNumbers = fNumbers;

  err = rs232_open(&hgsmctl->rs232, dev, GSM_BAUDRATE, RS232_FMT_8N1);
  if (err != 0) {
    goto bail;
  }

  rs232_blocking(hgsmctl->rs232, 0);
  
  /* Schedule cell phone reset */
  hgsmctl->reset_timeout = 1;

  /* Disable I/O lockout. */
  hgsmctl->no_io_timeout = 0;

  hgsmctl->flags = 0;

  *phgsmctl = hgsmctl;
  
  return 0;
  
bail:
  if (hgsmctl != NULL)
  {
    if (hgsmctl->rs232 != NULL)
      rs232_close(hgsmctl->rs232);
#ifdef USE_MALLOC
    free(hgsmctl);
#endif
  }
  return err;
}

void gsmctl_close(HANDLE_GSMCTL hgsmctl)
{
  rs232_close(hgsmctl->rs232);
  
#ifdef USE_MALLOC
  free(hgsmctl);
#endif
}
