/*
 * GSM remote control
 */

#include "gsmctl.h"
#include "rs232.h"
#include "machine.h"
#include "kernel.h"
#include <string.h>

#ifdef __MSP430_449__
#define GSM_BAUDRATE 19200
#else
#define GSM_BAUDRATE 9600
#endif

#define GSMCTL_ADDNUMBER 1
#define GSMCTL_GOTNUMBER 2

#define MAX_PHONE_NUMBERS 4

struct gsmctl{
  HANDLE_RS232 rs232;
  int no_io_timeout;
  int reset_timeout;
  gsmcallback cb;
  void *cb_data;
  char allowedNumbers[MAX_PHONE_NUMBERS][32];
  unsigned char flags;
  unsigned char gotIdx;
};

__attribute__ ((section (".infomem")))
const char fNumbers[MAX_PHONE_NUMBERS][32] =
{
  {0},{0},{0},{0}
};


static char tmp[32];

/*
 * Send AT command and get result.
 */
static int gsmctl_atio(HANDLE_GSMCTL hgsmctl, char *cmd, char *result)
{
  int bytes;
  
  rs232_write(hgsmctl->rs232, (unsigned char*)cmd, strlen(cmd));
  rs232_read(hgsmctl->rs232, (unsigned char*)tmp, strlen(cmd));
  
  bytes = rs232_read(hgsmctl->rs232, (unsigned char*)result, 32);

  if (bytes == 32) {  
    rs232_flush(hgsmctl->rs232);
  } 
  
  return bytes;
}


static
void gsmctl_init(HANDLE_GSMCTL hgsmctl)
{
  gsmctl_atio(hgsmctl, "atz\r", tmp);  
  gsmctl_atio(hgsmctl, "at+clip=1\r", tmp);
}

/*
 * Get one of the allowd phone numbers by index (start at 0).
 */
int gsmctl_getNumber(HANDLE_GSMCTL hgsmctl, char *out, int idx)
{
  if (idx < 0 || idx >= MAX_PHONE_NUMBERS)
    return -1;

  strncpy(out, hgsmctl->allowedNumbers[idx], 32);

  return 0;
}

/*
 * Add next incoming calling phone number to allowed list.
 */
void gsmctl_addNumber(HANDLE_GSMCTL hgsmctl)
{
  hgsmctl->flags &= ~GSMCTL_GOTNUMBER;
  hgsmctl->flags |= GSMCTL_ADDNUMBER;
  gsmctl_init(hgsmctl);
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
 * Save current allowed numbers into non-volatile memory
 */
void gsmctl_saveNumbers(HANDLE_GSMCTL hgsmctl)
{
  flash_write(fNumbers, hgsmctl->allowedNumbers, sizeof(fNumbers));
}

/*
 * Remove incoming calling phone number from allowed list.
 */
void gsmctl_removeNumber(HANDLE_GSMCTL hgsmctl, int idx)
{
  if (idx >= 0 && idx < MAX_PHONE_NUMBERS) {
    *hgsmctl->allowedNumbers[idx] = 0;
    flash_write(fNumbers, hgsmctl->allowedNumbers, sizeof(fNumbers));
  }
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

  /* Do reset from time to time to ensure GSM mobile phone is configured correctly */
  if ( hgsmctl->reset_timeout > 0 /*&& (hgsmctl->flags & GSMCTL_ADDNUMBER) == 0*/ ) {
    hgsmctl->reset_timeout--;
    if (hgsmctl->reset_timeout == 0) {
      gsmctl_init(hgsmctl);
      hgsmctl->reset_timeout = 120;
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
      if (*hgsmctl->allowedNumbers[x] == 0)
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
      if (*hgsmctl->allowedNumbers[x] != 0) {
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
int gsmctl_open(HANDLE_GSMCTL *phgsmctl, int dev)
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

  for (err=0; err<MAX_PHONE_NUMBERS; err++) {
    *hgsmctl->allowedNumbers[err] = 0;
  }

#if defined(__MSP430_169__) || defined(__MSP430_1611__) || defined(__MSP430_149__)
  /* Init Telit GSM modem */
  P6OUT |= BIT2;
  kernel_sleep(MSEC2JIFFIES(1000));
  kernel_yield();
  P6OUT |= BIT4;
  kernel_sleep(MSEC2JIFFIES(1000));
  kernel_yield();
  P6OUT &= ~BIT4;
#endif

  err = rs232_open(&hgsmctl->rs232, dev, GSM_BAUDRATE, RS232_FMT_8N1);
  if (err != 0) {
    goto bail;
  }

  rs232_blocking(hgsmctl->rs232, 0);
  
  memcpy(hgsmctl->allowedNumbers, fNumbers, sizeof(fNumbers));
  
  /* Schedule cell phone reset */
  hgsmctl->reset_timeout = 1;

  /* Disable I/O lockout. */
  hgsmctl->no_io_timeout = 0;

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

void gsmctl_addNumberCancel(HANDLE_GSMCTL hgsmctl)
{
  hgsmctl->flags = 0;
}

void gsmctl_close(HANDLE_GSMCTL hgsmctl)
{
  rs232_close(hgsmctl->rs232);
  
#ifdef USE_MALLOC
  free(hgsmctl);
#endif
}
