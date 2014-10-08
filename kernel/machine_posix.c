/*
 * POSIX specific things
 *
 * Author: Manuel Jander
 * License: BSD
 *
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <curses.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAX_FASTTIMERS 6
#define MAX_SLOWTIMERS 6

unsigned int machine_getJiffies(void)
{
  struct timeval t;
  gettimeofday(&t, NULL);
  return (t.tv_usec*JFREQ/1000000) + (t.tv_sec*JFREQ);
}

struct timer
{
  unsigned int ticks;
  unsigned int reload;
  int (*func)(HANDLE_TIMER, void*);
  void *data;
  unsigned char enabled;
};

static struct timer ftimers[MAX_FASTTIMERS];
static struct timer stimers[MAX_SLOWTIMERS];

HANDLE_TIMER machine_timer_create(int ival, timer_func func, void *data)
{
   int i, max;
   struct timer *t;
   
   if (ival < 0) {
     max = MAX_SLOWTIMERS;
     t = stimers;
     ival = -ival;
   } else {
     max = MAX_FASTTIMERS;
     t = ftimers;
   }
   
   for  (i=0; i<max; i++)
   {
     if (t[i].enabled == 0)
       break;
   }
   if (i==max)
     return NULL;
  
   t[i].func = func;
   t[i].data = data;
   t[i].ticks = t[i].reload = ival;
   t[i].enabled=1;
   
   return (HANDLE_TIMER)&t[i];
}

void machine_timer_destroy(HANDLE_TIMER hTimer)
{
  struct timer *timer = (struct timer*) hTimer;
  
  timer->enabled = 0;
}

void machine_timer_reset(HANDLE_TIMER hTimer, int ival)
{
  hTimer->reload = ival;
}


static rtc_time_t rtc_now, rtc_alarm;
static rtc_alarm_callback rtc_alarm_cb;
static void *rtc_alarm_data;

signed char rtc_carry(signed char *val, signed char limit)
{
  signed char carry = 0;
  
  while (*val >= limit)
  {
    *val -= limit;
    carry++;
  }
  while (*val < 0)
  {
    *val += limit;
    carry--;
  }
  return carry;
}

void rtc_add(rtc_time_t *t, signed char secs)
{
  t->seconds += secs; 
  t->minutes += rtc_carry(&t->seconds, 60);
  t->hours   += rtc_carry(&t->minutes, 60);
#ifndef USE_CALENDAR
                rtc_carry(&t->hours, 24);
#else
  t->day     += rtc_carry(&t->hours, 24);
  t->month   += rtc_carry(&t->day, 31); /* FIXME: This is not correct */
  t->year    += rtc_carry(&t->month, 12);
#endif  
}

static int rtc_time_isequal(rtc_time_t *a, rtc_time_t *b)
{
  if (a->seconds != b->seconds) return 0; 
  if (a->minutes != b->minutes) return 0; 
  if (a->hours != b->hours) return 0; 
#ifdef USE_CALENDAR
  if (a->day != b->day) return 0; 
  if (a->month != b->month) return 0; 
  if (a->year != b->year) return 0; 
#endif

  return 1;
}

void rtc_tick(int val)
{
  int wup = 0;

  rtc_add(&rtc_now, 1);
  if (rtc_time_isequal(&rtc_now, &rtc_alarm))
  {
    if (rtc_alarm_cb != NULL)
      rtc_alarm_cb(rtc_alarm_data);
  }

  /* Slow timers */
#if (MAX_SLOWTIMERS > 0)
  {
    int i;

    for (i=0; i<MAX_SLOWTIMERS; i++)
    {
      if (stimers[i].enabled)
      {
        stimers[i].ticks--;
        if (stimers[i].ticks==0)
        {
          stimers[i].ticks = stimers[i].reload;
          wup |= stimers[i].func((HANDLE_TIMER)&stimers[i], stimers[i].data);
        }
      }  
    }    
  }      
#endif   

}

void machine_basic_timer_isr(int val)
{
  static int count = 0;
  int i;
  int wup = 0;
  
  for (i=0; i<MAX_FASTTIMERS; i++)
  {
    if (ftimers[i].enabled)
    {
      ftimers[i].ticks--;
      if (ftimers[i].ticks==0)
      {
        ftimers[i].ticks = ftimers[i].reload;
        wup |= ftimers[i].func((HANDLE_TIMER)&ftimers[i], ftimers[i].data);
      }
    }  
  }

  if (count++ > TFREQ) {
    count = 0;
    rtc_tick(val);
  }
}

static void rtc_init(void)
{  
  rtc_alarm_cb = NULL;
}

void rtc_setclock(rtc_time_t *t)
{
  memcpy(&rtc_now, t, sizeof(rtc_time_t));
}

void rtc_getclock(rtc_time_t *t)
{
  memcpy(t, &rtc_now, sizeof(rtc_time_t));
}

void rtc_setalarm(rtc_time_t *t, rtc_alarm_callback cb, void *data)
{
  memcpy(&rtc_alarm, t, sizeof(rtc_time_t));
  rtc_alarm_cb = cb;
  rtc_alarm_data = data;
}

void rtc_getalarm(rtc_time_t *t)
{
  memcpy(t, &rtc_alarm, sizeof(rtc_time_t));
}

#include "wbus_server.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

static int shmid;
static unsigned short (*d)[16];

void machine_init(void)
{
  int i;

  rtc_init();
  
  shmid = shmget(0xb0E1, 4096, 0666 | IPC_CREAT);
  if (shmid == -1) {
    printf("shmget() failed\n");
  }
  d = shmat(shmid, (void *)0, 0);
  if ((int)d == -1) {
    printf("shmat() failed (d)\n");
  }

  /* Set all timers disabled */
#if (MAX_FASTTIMERS > 0)   
  for (i=0; i<MAX_FASTTIMERS; i++) {
    ftimers[i].enabled = 0;
  }
#endif
#if (MAX_SLOWTIMERS > 0)
  for (i=0; i<MAX_SLOWTIMERS; i++) {
    stimers[i].enabled = 0;
  }
#endif

  adc_invalidate();
  
  /* Add signals for timers */
  //signal(SIGALRM, rtc_tick);
  signal(SIGALRM, machine_basic_timer_isr);
  {
    struct itimerval it;
    int err;
#if 0
    it.it_interval.tv_sec = 1;
    it.it_interval.tv_usec = 0000000;
    it.it_value.tv_sec = 1;
    it.it_value.tv_usec = 000000;
#else
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 1000000/TFREQ;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = 1000000/TFREQ;
#endif
    err = setitimer(ITIMER_REAL, &it, NULL);
    if (err != 0) {
      printf("Error setitimer: %s\n", strerror(err));
    }
  }  
}

#include <sys/select.h>
#include <sys/time.h>

void machine_usleep(unsigned long us) /* 9+a*12 cycles */
{
  struct timeval timeout;
  
  timeout.tv_sec = 0;
  timeout.tv_usec = us;
  
#ifdef __linux__
  while (timeout.tv_sec > 0 || timeout.tv_usec > 0)
#endif
  {
    select(0, NULL, NULL, NULL, &timeout);
  }
}

void machine_msleep(unsigned int b)
{
  machine_usleep(b*1000);
}
    
void machine_sleep(unsigned int b)
{
  sleep(b);
}

void machine_jsleep(unsigned int j)
{
  machine_usleep(j*1000000/JFREQ);
}

void machine_beep(void)
{
  printf("\b");
}

void machine_led_set(int s)
{
}

void machine_backlight_set(int en)
{
}

int machine_backlight_get(void)
{
  return 0; 
}

unsigned int machine_buttons(int do_read)
{
  char key = 0;
  struct timeval tv = { 0L, 0L };
  fd_set fds;
  FD_SET(0, &fds);
    
  if (select(1, &fds, NULL, NULL, &tv)) {
    if (do_read) {
      read(fileno(stdin), &key, 1);
    } else {
      key = 1;
    }
  }

  return (unsigned int)key;
}            

void machine_lcd(int enable)
{
  
}

int adc_read_single(int c)
{
  return d[1][c];
}

void adc_read(unsigned short *s, int n)
{
  int i;
  /* Write back virtual sensors */
  d[1][10] = s[10];
  d[1][9] = s[9];

  for (i=0; i<n; i++) {
    s[i] = d[1][i];
  }
}

void adc_invalidate(void)
{
  d[2][0] = 0;
}
   
int adc_is_uptodate(void)
{
  return d[2][0];
}

void machine_act(unsigned short a[], int n)
{
  int i;

  for (i=0; i<n; i++) {
    d[0][i] = a[i];
  }  
}

void machine_ack(int ack)
{

}

void adc_makeText(char *str, int channel, int value)
{
  sprintf(str, "%4d", value);

  if (channel > 1) {
    str[4] = str[3];
    str[3] = '.';
  }  
}

int machine_stayAwake(void)
{
  return 0;
}

/*
 * Put the machine to sleep in lowest power mode
 */
void machine_suspend(void)
{
  sleep(1);
}

void flash_write(void *_fptr, void *_rptr, int nbytes)
{
  if (nbytes == 0) {
    memset(_fptr, 0, 128);
    return;
  }
  if (_rptr != NULL) {
    memcpy(_fptr, _rptr, nbytes);
  } else {
    memset(_fptr, 0, nbytes);
  }
}
