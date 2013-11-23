#include "machine.h"
#include "kernel.h"
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#define TS_SUSPEND  1    /* Suspended until explicit wakeup */
#define TS_SLEEP    2    /* Suspended until timeout         */
#define TASK_RUNNING(t)  ((t.flags & TS_SUSPEND) == 0)
#define TASK_SLEEPING(t) (t.flags & TS_SLEEP)

#define min(a,b) (((a)<(b))?(a):(b))

typedef struct {
  jmp_buf jb;
  kernel_task_t func;
  unsigned int st;
  unsigned int stack_size;
  unsigned char flags;
} uTask_t;

static jmp_buf kTask;                   /* Kernel task buffer         */
static uTask_t uTask[KERNEL_MAX_TASK];  /* User task context struct   */
static unsigned int iTask;              /* Current user task          */
static unsigned int nTask;		 /* Number of registered tasks */
static int uStack[KERNEL_STACK_SIZE];

void kernel_init(void)
{
  machine_init();
  memset(uTask, 0, sizeof(uTask));
  memset(uStack, 0, sizeof(uStack)); 
  nTask = 0;
}

#if defined(__linux__) || defined(_WIN32)
static int gKernelRunning=0;
int kernel_running(void)
{
  return gKernelRunning;     
}
#endif

/* Suspend a given task */
void kernel_suspend(void)
{
  uTask[iTask].flags |= TS_SUSPEND;
}

void kernel_sleep(unsigned int j)
{
  uTask[iTask].st = j;
  uTask[iTask].flags |= TS_SLEEP;
}

void kernel_yield(void)
{
  if (my_setjmp(uTask[iTask].jb) == 0) {
    my_longjmp(kTask, 1);
  }    
}

void kernel_wakeup(int task)
{
  if (task == -1) {
    int i;
    
    for (i=0; i<nTask; i++) {
      uTask[i].flags &= ~(TS_SLEEP|TS_SUSPEND);
    }
  } else {
    uTask[task].flags &= ~(TS_SLEEP|TS_SUSPEND);
  }
}

unsigned int kernel_getTask(void)
{
  return iTask;
}

int kernel_task_register(kernel_task_t task, int stack_size)
{
  uTask[nTask].func = task;
  uTask[nTask].flags = 0;
  uTask[nTask].stack_size = stack_size;
  nTask++;
  
  return nTask-1;
}

static unsigned int pct = 0;

unsigned char kernel_get_next_task(unsigned char t)
{
  unsigned int ct, kt;
  unsigned char i;
  
  do {
    kt = machine_getJiffies();
    ct = kt - pct;
    pct = kt;
    kt = (unsigned int)0xffffffff;

    /* decrement sleep time counters */
    for (i=0; i<nTask; i++) {
      if (uTask[i].st < ct) {
        uTask[i].st = 0;
      } else {
        uTask[i].st -= ct;
      }
    }

    for (i=nTask; i!=0; i--) {
      t++;
      if (t>=nTask) {
        t=0;
      }    
      if ( TASK_SLEEPING(uTask[t]) ) {
        if ( uTask[t].st == 0 ) {
          uTask[t].flags &= ~TS_SLEEP;
          kt = 0;
          break;
        }
        /* task is sleeping, see if it is time to wake it up */
        kt = min(uTask[t].st, kt);
      } else {
        if (TASK_RUNNING(uTask[t])) {
          /* task i is not sleeping, so take it */
          kt = 0;
          break;
        }
      }
    }

    if (kt != 0) {
      /* nothing to do until kt jiffies */
      machine_jsleep(kt);
    }

  } while ( (!TASK_RUNNING(uTask[t])) || TASK_SLEEPING(uTask[t]));
  
  return t;
}

void kernel_run_threads(void)
{
  iTask = nTask-1;
  while (1) {
    iTask = kernel_get_next_task(iTask);
    if ( my_setjmp(kTask) == 0) {
      my_longjmp(uTask[iTask].jb, 1);
    }
  }
}

void kernel_run(void)
{
  int *pStack = uStack+KERNEL_STACK_SIZE-1;

#if defined(__linux__) || defined(_WIN32)
  gKernelRunning = 1;
#endif

  for (iTask=0; iTask<nTask; iTask++) {
    if ( my_setjmp(kTask) == 0 ) {
      setup_task(pStack, uTask[iTask].func);
    }
    pStack -= uTask[iTask].stack_size;
  }  

  kernel_run_threads();  
}
