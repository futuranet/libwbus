/*
 * RS232 portable access layer
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#include "kernel.h"
#include "machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <string.h>

#include "signal.h"
#include <sys/ioctl.h> 


struct RS232
{
    int fd;
    int dev;
    unsigned char block;
    unsigned char task;
    volatile int rx_waitbytes;
};

HANDLE_RS232 grs232[NO_RS232];

static
void rs232_isr(int signum)
{
  int result, err, i;

  for (i=0; i<NO_RS232; i++) {
    /* skip rs232 ports that are not opened */
    if (grs232[i] == NULL) {
      continue ;
    }
    err = ioctl(grs232[i]->fd, FIONREAD, &result);
    if (err == -1) {
      printf("ioctl failed\n");
      continue;
    }
    if (result < grs232[i]->rx_waitbytes) {
      printf("received %d bytes\n", result);  
      continue;
    }
    break;
  }

  if (i == NO_RS232) {
    return;
  }

  kernel_wakeup(grs232[i]->task);
  grs232[i]->rx_waitbytes = 0;
}

static
void rs232_isr_start(HANDLE_RS232 rs232)
{
  int err;

  signal(SIGIO, rs232_isr);
  
  err = fcntl(rs232->fd, F_SETOWN, getpid());
  if (err < 0) {
    printf("F_SETOWN failed\n");
  }

  err = fcntl(rs232->fd, F_GETFL);
  err = fcntl(rs232->fd, F_SETFL, err | FASYNC);
  if (err < 0) {
    printf("FIOASYNC start failed\n");
  }
  
  grs232[rs232->dev] = rs232;
}

static
void rs232_isr_end(HANDLE_RS232 rs232)   
{
  int err;

  signal(SIGIO, SIG_DFL);

  err = fcntl(rs232->fd, F_GETFL);
  err = fcntl(rs232->fd, F_SETFL, err & ~FASYNC);
  if (err < 0) {
    printf("FIOASYNC end failed\n");
  }
  
}

static speed_t baud(long baudrate)
{
  int baud;

  switch (baudrate) 
  {
    case 50: baud=B50; break;
    case 75: baud=B75; break;
    case 110: baud=B110; break;
    case 134: baud=B134; break;
    case 150: baud=B150; break;
    case 200: baud=B200; break;
    case 300: baud=B300; break;
    case 600: baud=B600; break;
    case 1200: baud=B1200; break;
    case 2400: baud=B2400; break;
    case 4800: baud=B4800; break;
    case 9600: baud=B9600; break;
    case 19200: baud=B19200; break;
    case 38400: baud=B38400; break;
    case 57600: baud=B57600; break;
    case 115200: baud=B115200; break;
    case 230400: baud=B230400; break;
    default: baud=B0; break;
  }
  return baud;
}

int rs232_open(HANDLE_RS232 *pRs232, unsigned char dev_idx, long baudrate, unsigned char format)
{
    HANDLE_RS232 rs232 = NULL;
    struct termios tio;
    char device[16];
    char *edp;

    rs232 = (HANDLE_RS232)malloc(sizeof(struct RS232));
    if (rs232 == NULL) {
       printf("malloc error\n");
       goto bail;
    }

    sprintf(device, "WBSERDEV%d", dev_idx);
    edp = getenv(device);
    if (edp != NULL) {
      strcpy(device, edp);
    } else {
      edp = getenv("WBSERDEV");
      if (edp != NULL) {
        strcpy(device, edp);
      } else {
        strcpy(device, "/dev/ttyS0");
      }
      device[strlen(device)-1] = (char)('0' + dev_idx);
    }

    printf("RS232 device %s\n", device);

    rs232->fd = open (device, O_SYNC | O_RDWR | O_NOCTTY );
    if (rs232->fd < 0) {
      printf("Unable to open device %s\n", device);
      goto bail; 
    }
    rs232->dev = dev_idx;

    tcgetattr (rs232->fd, &tio);
    tio.c_cflag = baud(baudrate) | CS8 | CLOCAL | CREAD;
    tio.c_iflag = IGNBRK;

    switch (format) {
      case RS232_FMT_8E1:
        tio.c_cflag |= PARENB;
        tio.c_iflag |= INPCK;
        break;
      case RS232_FMT_8O1:
        tio.c_cflag |= PARENB | PARODD;
        tio.c_iflag |= INPCK;
        break;
    }
    tio.c_lflag = NOFLSH ;
    tio.c_oflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 10; /* Set timeout to 1000ms */
    tcflush (rs232->fd, TCIFLUSH);
    tcsetattr (rs232->fd, TCSANOW, &tio);

    *pRs232 = rs232; 

    return 0;
    
bail:
    rs232_close(rs232);
    return -1;
}

void rs232_close(HANDLE_RS232 rs232)
{
    if (rs232 != NULL)
    {
        if (rs232->fd > 0)
             close(rs232->fd);
        free (rs232);
    }    
}

static int rs232_bytes_avail(HANDLE_RS232 rs232)
{
  int result;
  ioctl(rs232->fd, FIONREAD, &result);
  return result;
}

unsigned char rs232_read(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes)
{
  int n;

  if (kernel_running()) {
    if (rs232_bytes_avail(rs232) < nbytes) {
      rs232->task = kernel_getTask();
      rs232->rx_waitbytes = nbytes-rs232_bytes_avail(rs232);
      rs232_isr_start(rs232);
      if (rs232->block) {
        kernel_suspend();
      } else {
        kernel_sleep( MSEC2JIFFIES(1000) | (int)rs232->rx_waitbytes<<4);
      }
      kernel_yield();
      rs232_isr_end(rs232);
    }

    /* See how many bytes are available now */
    n = rs232_bytes_avail(rs232);
    if (n > nbytes) { 
      n = nbytes;
    }
    n = read(rs232->fd, data, n);
  } else {
    int left;
    left = nbytes;
    
    while (left > 0) {
     n = read(rs232->fd, data, left);
     if (n <= 0) {
       break;
     }
     left -= n;
     data += n;
    }
    n = nbytes-left;
  }

  return n;
}

int rs232_rxBytes(HANDLE_RS232 rs232)
{
  return rs232_bytes_avail(rs232);
}

void rs232_write(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes)
{
  /* tcflush(rs232->fd, TCIFLUSH); */
  write(rs232->fd, data, nbytes);
}

void rs232_sbrk(HANDLE_RS232 rs232, unsigned char state)
{
  if (state) {
    ioctl(rs232->fd, TIOCSBRK, NULL);
  } else {
    ioctl(rs232->fd, TIOCCBRK, NULL);
  }
}

void rs232_flush(HANDLE_RS232 rs232)
{
  tcflush(rs232->fd, TCIOFLUSH);
}

void rs232_blocking(HANDLE_RS232 rs232, unsigned char block)
{
  rs232->block = block;
}
