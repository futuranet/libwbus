/**
 * \file openegg.c
 * Description : Webasto (tm) car heating device control application for embedded devices
 * Author : Manuel Jander
 * 
 * The name openegg comes from the term "Eieruhr", because of the shape of the original
 * Webasto (tm) controlling clock. 
 */

#include "wbus.h"
#include "openegg_ui.h"
#include "openegg_menu.h"
#include "gsmctl.h"
#include "kernel.h"
#include "machine.h"
#include <stdio.h>
#include <string.h>

/* Display active timeout until power down in seconds */
#define ACTIVE_TIMEOUT 60

typedef enum {
  MENU_MONITOR_T0 = 1,
  MENU_MONITOR_T1,
  MENU_MONITOR_T2,
  MENU_MONITOR_P0,
  MENU_MONITOR_P1,
	
  MENU_GSM_ADD,
  MENU_GSM_REMOVE,
  MENU_GSM_SAVE,

  MENU_TIMER_SELECT,
  MENU_TIMER_SETALARM0,
  MENU_TIMER_SETALARM1,
  MENU_TIMER_SETALARM2,
  MENU_TIMER_SETTIME,
  MENU_TIMER_SHOWTIME,
	
  MENU_HEATER_SENS_S0,
  MENU_HEATER_SENS_S1,
  MENU_HEATER_SENS_S2,
  MENU_HEATER_SENS_S3,
  MENU_HEATER_SENS_S4,
  MENU_HEATER_SENS_S5,
  MENU_HEATER_SENS_S6,
  MENU_HEATER_SENS_S7,
  MENU_HEATER_SENS_S8,
  MENU_HEATER_SENS_S9,
  MENU_HEATER_SENS_S10,
  MENU_HEATER_SENS_S11,
  MENU_HEATER_SENS_S12,
  MENU_HEATER_SENS_S13,
  MENU_HEATER_SENS_S14,
  MENU_HEATER_SENS_S15,
  MENU_HEATER_SENS_S16,
  MENU_HEATER_SENS_S17,
  MENU_HEATER_SENS_S18,
  MENU_HEATER_SENS_S19,
	
  MENU_HEATER_CTRL_OFF,
  MENU_HEATER_CTRL_PH,
  MENU_HEATER_CTRL_SH,
  MENU_HEATER_CTRL_VENT,
  MENU_HEATER_CTRL_FP,

  MENU_HEATER_SETMODE_PH,
  MENU_HEATER_SETMODE_SH,
  MENU_HEATER_SETMODE_VENT,
  MENU_HEATER_SETMODE_FP,
  
  MENU_HEATER_SETTIME,
	
  MENU_HEATER_EC_SHOW,
  MENU_HEATER_EC_CLEAR,
	
  MENU_HEATER_IDENT
	
} state_id; 

/* Non Menu end points require forward declarations for go back entries */
const mstate_t menu[4];
const mstate_t menu_heater[7];

const mstate_t menu_monitor[6] =
{
  { NULL, MENU_MONITOR_T0, "T Oel" },
  { NULL, MENU_MONITOR_T1, "T ESP" },
  { NULL, MENU_MONITOR_T2, "T uC" },
  { NULL, MENU_MONITOR_P0, "Ladedruck" },
  { NULL, MENU_MONITOR_P1, "Sog" },
  { menu, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_gsm[4] =
{
  { NULL, MENU_GSM_ADD, "Neue Nummer" },
  { NULL, MENU_GSM_SAVE, "Speichern" },
  { NULL, MENU_GSM_REMOVE | MENU_ITER, "Entfernen" },
  { menu, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_timer[7] =
{
  { NULL, MENU_TIMER_SELECT | MENU_ITER, "Timer selektieren" },
  { NULL, MENU_TIMER_SETALARM0 | MENU_ITER, "Timer 1 einstellen" },
  { NULL, MENU_TIMER_SETALARM1 | MENU_ITER, "Timer 2 einstellen" },
  { NULL, MENU_TIMER_SETALARM2 | MENU_ITER, "Timer 3 einstellen" },
  { NULL, MENU_TIMER_SETTIME | MENU_ITER, "Zeit einstellen" },
  { NULL, MENU_TIMER_SHOWTIME, "Zeit anzeigen" },
  { menu, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_heater_sens[14] =
{
  /* { NULL, MENU_HEATER_SENS_S0, "SH sensor 0" }, */
  /* { NULL, MENU_HEATER_SENS_S1, "SH sensor 1" }, */
  { NULL, MENU_HEATER_SENS_S2, "SH sensor 2" },
  { NULL, MENU_HEATER_SENS_S3, "SH sensor 3" },
  { NULL, MENU_HEATER_SENS_S4, "SH sensor 4" },
  { NULL, MENU_HEATER_SENS_S5, "SH sensor 5" },
  { NULL, MENU_HEATER_SENS_S6, "SH sensor 6" },
  { NULL, MENU_HEATER_SENS_S7, "SH sensor 7" },
  /* { NULL, MENU_HEATER_SENS_S8, "SH sensor 8" }, */
  /* { NULL, MENU_HEATER_SENS_S9, "SH sensor 9" }, */
  { NULL, MENU_HEATER_SENS_S10, "SH sensor 10" },
  { NULL, MENU_HEATER_SENS_S11, "SH sensor 11" },
  { NULL, MENU_HEATER_SENS_S12, "SH sensor 12" },
  /* { NULL, MENU_HEATER_SENS_S13, "SH sensor 13" }, */
  /* { NULL, MENU_HEATER_SENS_S14, "SH sensor 14" }, */
  { NULL, MENU_HEATER_SENS_S15, "SH sensor 15" },
  /* { NULL, MENU_HEATER_SENS_S16, "SH sensor 16" }, */
  { NULL, MENU_HEATER_SENS_S17, "SH sensor 17" },
  { NULL, MENU_HEATER_SENS_S18, "SH sensor 18" },
  { NULL, MENU_HEATER_SENS_S19, "SH sensor 19" },
  { menu_heater, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_heater_ctrl[6] =
{
  { NULL, MENU_HEATER_CTRL_OFF, "Aus" },
  { NULL, MENU_HEATER_CTRL_PH, "Heizen" },
  { NULL, MENU_HEATER_CTRL_SH, "Zuheizen" },
  { NULL, MENU_HEATER_CTRL_VENT, "Luft" },
  { NULL, MENU_HEATER_CTRL_FP, "Entlueften" },
  { menu_heater, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_heater_setmode[4] =
{
  { NULL, MENU_HEATER_SETMODE_PH, "Heizen" },
  { NULL, MENU_HEATER_SETMODE_SH, "Zuheizen" },
  { NULL, MENU_HEATER_SETMODE_VENT, "Luft" },
  { menu_heater, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_heater_ec[3] =
{
  { NULL, MENU_HEATER_EC_SHOW | MENU_ITER, "Fehler Anzeigen" },
  { NULL, MENU_HEATER_EC_CLEAR, "Fehler wech" },
  { menu_heater, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu_heater[7] =
{
  { menu_heater_ctrl, MENU_NAV, "SH Steuern" },
  { NULL, MENU_HEATER_SETTIME | MENU_ITER, "Laufzeit" },
  { menu_heater_setmode, MENU_NAV, "SH Mode" },
  { menu_heater_sens, MENU_NAV, "SH Sensoren"},
  { NULL, MENU_HEATER_IDENT | MENU_ITER, "SH Ident" },
  { menu_heater_ec, MENU_NAV, "SH Fehler" },
  { menu, MENU_NAV | MENU_END, "<---"}
};

const mstate_t menu[4] = 
{ 
  { menu_monitor, MENU_NAV, "Anzeige" }, 
  { menu_heater, MENU_NAV, "Heizung" },
  { menu_timer, MENU_NAV, "Timer" },
  { menu_gsm, MENU_NAV | MENU_END, "GSM" },
};

#if defined(__MSP430_449__)
#define WBUS_DEV 1
#define GSM_DEV 0
#else
#define WBUS_DEV 0
#define GSM_DEV 1
#endif

/*  libwbus data */

/* GSM control data */
static HANDLE_GSMCTL hgsmctl;
/* Request heater on */
static signed int fHeaterOn;
/* Index of timer for heater to be enabled (1-3) or timer off (0) */
static unsigned char fTimerOn;
/* current heater mode */
static unsigned char heaterMode;
static int wbus_task;

/* Heater/Vent time in Minutes */
typedef struct {
  signed char heatTime;
  unsigned char heaterMode;
  rtc_time_t alarm[3];
  //unsigned char padding[117];
} settings_t;

static settings_t settings;

__attribute__ ((section (".flashrw")))
settings_t fsettings = 
{
  20,
  WBUS_PH,
  { { 0, 0, 7 },{ 0, 0, 12 },{ 0, 0, 19 } },
};

static rtc_time_t *ptime;
static rtc_time_t openegg_time;

static void turn_on_heater(void)
{
  if (fHeaterOn == 0) {
    machine_beep();
    fHeaterOn = 1;
    kernel_wakeup(wbus_task);
  }
}

static void turn_on_heater_gsm(HANDLE_GSMCTL hgsmctl, void *data)
{
  heaterMode = settings.heaterMode;
  turn_on_heater();
}

static void turn_on_heater_alarm(void *data)
{
  heaterMode = settings.heaterMode;
  turn_on_heater();
  /* Disable timer in order to avoid the heater from starting every day. */
  rtc_setalarm(&settings.alarm[fTimerOn-1], NULL, NULL);
  fTimerOn = 0;
}

static void turn_off_heater()
{
  fHeaterOn = -1;
  if (fTimerOn > 0) {
    /* Disable timer. */
    rtc_setalarm(&settings.alarm[fTimerOn-1], NULL, NULL);
  }
}

static int read_adc_and_convert(char *str, unsigned char channel)
{
  unsigned short tmp;

  tmp = adc_read_single(channel);

  if (str != NULL) {
    sprintf(str, "%4d", (int)tmp);    

    if (channel > 1) {
      str[4] = str[3];
      str[3] = '.';
    }
  }
  return (int)tmp;
}

/* This structs are temporal but are too large to place them on a task stack */
static union {
  wb_info_t wb_info;
  wb_errors_t wb_errors;
  wb_sensor_t wb_sensors;
} wbdata;
static int icnt;

static void openegg_do(int cmd, int *pflags, char *text, char *digits)
{
  HANDLE_WBUS wbus = NULL;
  int flags = *pflags;
  int err = 0;

  /* iterator helper */
  if (flags & ITER_START)
    icnt = 0;
  if (flags & ITER_NEXT)
    icnt++;
  if (flags & ITER_BACK)
    icnt--;  
  
  switch (cmd)
  {
    /* Generic commands */
    case MENU_MONITOR_T0:
    case MENU_MONITOR_T1:
    case MENU_MONITOR_P0:
    case MENU_MONITOR_P1:
    case MENU_MONITOR_T2:
      flags |= CMD_STICKY;
      flags |= DISP_DIGITS;
      break;

    /* Timer/Clock */
    case MENU_TIMER_SELECT:
      flags &= ~CMD_STICKY;
      if (flags & ITER_NEXT) {
        if (icnt > 3) icnt = 0;        
      }
      ptime = &settings.alarm[icnt-1];
      if (icnt > 0) {
        if (flags & ITER_ACK) {
          /* Set new current alarm index */
          fTimerOn = icnt;
          /* Set active alarm */
          rtc_setalarm(ptime, turn_on_heater_alarm, text);
        }
        sprintf(text, "T %d AN", icnt);
      } else {
        if (flags & ITER_ACK) {
          /* Disable previously possibly enabled alarm */
          rtc_setalarm(&settings.alarm[fTimerOn-1], NULL, NULL);
          /* Set alarm disabled. */
          fTimerOn = 0;
        }
        strcpy(text, "Timer AUS");
        ptime = NULL;
      }
      flags |= DISP_TEXT; 
      break;
    case MENU_TIMER_SETALARM0:
    case MENU_TIMER_SETALARM1:
    case MENU_TIMER_SETALARM2:
      flags &= ~CMD_STICKY;
      if (flags & ITER_START) {
        ptime = &settings.alarm[cmd-MENU_TIMER_SETALARM0];
      }
      sprintf(text, "Timer %d", cmd-MENU_TIMER_SETALARM0+1);
      flags |= DISP_TEXT; 
      if (flags & ITER_ACK) {
        flash_write(&fsettings, &settings, sizeof(settings_t));
      }
      break;
    case MENU_TIMER_SETTIME:
      flags &= ~CMD_STICKY;
      if (flags & ITER_START) {
        ptime = &openegg_time;
        rtc_getclock(ptime);
      }
      if (flags & ITER_ACK) {
        rtc_setclock(ptime);
        //goto bail;
      }
      strcpy(text, "Zeit");
      flags |= DISP_TEXT;
      break;
    case MENU_TIMER_SHOWTIME:
      flags |= CMD_STICKY;
      ptime = &openegg_time;
      rtc_getclock(ptime);
      break;

    /* heater commands */
    case MENU_HEATER_IDENT:
    case MENU_HEATER_EC_SHOW:
      if (!(flags & ITER_START)) {
        /* Requires opened wbus connection only at iteration start. */
        break;
      }
    case MENU_HEATER_EC_CLEAR:
    case MENU_HEATER_CTRL_OFF:
    case MENU_HEATER_CTRL_FP:
    case MENU_HEATER_SENS_S0:
    case MENU_HEATER_SENS_S1:
    case MENU_HEATER_SENS_S2:
    case MENU_HEATER_SENS_S3:
    case MENU_HEATER_SENS_S4:
    case MENU_HEATER_SENS_S5:
    case MENU_HEATER_SENS_S6:
    case MENU_HEATER_SENS_S7:
    case MENU_HEATER_SENS_S8:
    case MENU_HEATER_SENS_S9:
    case MENU_HEATER_SENS_S10:
    case MENU_HEATER_SENS_S11:
    case MENU_HEATER_SENS_S12:
    case MENU_HEATER_SENS_S13:
    case MENU_HEATER_SENS_S14:
    case MENU_HEATER_SENS_S15:
    case MENU_HEATER_SENS_S16:
    case MENU_HEATER_SENS_S17:
    case MENU_HEATER_SENS_S18:
    case MENU_HEATER_SENS_S19:
      err = wbus_open(&wbus, WBUS_DEV);
      if (err) {
        goto bail;
      }
      break;
  }

  switch (cmd)
  {
    /* Generic commands */
    case MENU_MONITOR_T0:
      read_adc_and_convert(digits, 2);
      break;
    case MENU_MONITOR_T1:
      read_adc_and_convert(digits, 3);
      break;
    case MENU_MONITOR_T2:
      read_adc_and_convert(digits, 4);
      break;
    case MENU_MONITOR_P0:
      read_adc_and_convert(digits, 0);
      break;
    case MENU_MONITOR_P1:
      read_adc_and_convert(digits, 1);
      break;
  
    /* Timer/Clock */
    case MENU_TIMER_SETTIME:
    case MENU_TIMER_SETALARM0:
    case MENU_TIMER_SETALARM1:
    case MENU_TIMER_SETALARM2:
      if (flags & ITER_NEXT)
        rtc_add(ptime, 60);
      if (flags & ITER_BACK)
        rtc_add(ptime, -60);
    case MENU_TIMER_SELECT:
    case MENU_TIMER_SHOWTIME:
      if (ptime != NULL)
        sprintf(digits, "%02d:%02d", ptime->hours, ptime->minutes);
      else
        strcpy(digits, "--:--");
      flags |= DISP_DIGITS;
      break;      

    case MENU_GSM_ADD:
      if ( (flags & CMD_STICKY) == 0) {
        gsmctl_addNumber(hgsmctl);
        strcpy(text, "Ruf an");
        flags |= DISP_TEXT|CMD_STICKY;
      } else {
        if (gsmctl_getNewNumber(hgsmctl, text)) {
          flags |= DISP_TEXT;
          flags &= ~CMD_STICKY;
        }
        //if (flags & ITER_ACK) {
        //  gsmctl_addNumberCancel(hgsmctl);
        //  flags &= ~CMD_STICKY;
        //}
      }
      break;
    case MENU_GSM_REMOVE:
      flags &= ~CMD_STICKY;
      if (flags & ITER_ACK) {
        gsmctl_removeNumber(hgsmctl, icnt);
        gsmctl_saveNumbers(hgsmctl);
      }
      err = gsmctl_getNumber(hgsmctl, text, icnt);
      if (err) {
        if (icnt != 0) {
          icnt = 0;
          err = gsmctl_getNumber(hgsmctl, text, 0);
        }
      }
      sprintf(digits, "%4d", icnt+1);
      if (*text == 0) {
        strcpy(text, "Leer");
      }
      err = 0;
      flags |= DISP_TEXT|DISP_DIGITS;
      break;
    case MENU_GSM_SAVE:
      flags &= ~CMD_STICKY;
      gsmctl_saveNumbers(hgsmctl);
      break;

    /* Heater commands */    
    case MENU_HEATER_SETTIME:
      flags &= ~CMD_STICKY;
      if (flags & ITER_START) {
        icnt = settings.heatTime;
      }
      if (flags & ITER_ACK) {
        settings.heatTime = icnt;
        flash_write(&fsettings, &settings, sizeof(settings_t));
      }
      strcpy(text, "Minuten");
      flags |= DISP_DIGITS|DISP_TEXT;
      sprintf(digits, "  %2d", icnt);
      if (icnt > 58)
        icnt = -1;
      if (icnt < 1)
        icnt = 1;
      break;
    case MENU_HEATER_IDENT:
      if (flags & ITER_START) {
        err = wbus_get_wbinfo(wbus, &wbdata.wb_info);
        if (err) {
          goto bail;
        }
      }
      /* Retrieve device indenty info */      
      wbus_ident_print(text, &wbdata.wb_info, icnt);
      flags |= DISP_TEXT;
      /* Stop after last info string or ack button. */
      if ((icnt+1 > WB_IDENT_LINES) || (flags & ITER_ACK)) {
      	flags &= ~CMD_STICKY;
      }
      break;
		
    case MENU_HEATER_EC_SHOW:
      /* read error codes  */
      if (flags & ITER_START) {
     	err = wbus_errorcodes_read(wbus, &wbdata.wb_errors);
     	if (err) {
     	  goto bail;
        }
      }
      /* Wrap around */
      if (icnt < 0) {
        icnt = wbdata.wb_errors.nErr-1;
      }
      if (icnt >= wbdata.wb_errors.nErr) {
        icnt = 0;
      }
      wbus_errorcode_print(text, &wbdata.wb_errors, icnt);
      sprintf(digits, "   %d", icnt);
      flags |= DISP_TEXT|DISP_DIGITS;
      /* Stop after last info string or ack button. */
      if ( flags & ITER_ACK ) {
      	flags &= ~CMD_STICKY;
      }
      break;

    case MENU_HEATER_EC_CLEAR:
      /* Clear error codes */
      err = wbus_errorcodes_clear(wbus);
      if (err != 0)
        goto bail;
      strcpy(text, "Hurra!");
      flags |= DISP_TEXT;
      break;
#if 0
    case CMD_TEST:
      printf("Testing device %d for %d seconds with %3.2f percent or Hertz\n", test, tim, tval);
      err = wbus_test(wbus, test, tim, tval);
      break;
#endif

    case MENU_HEATER_CTRL_VENT:
      heaterMode = WBUS_VENT;
      turn_on_heater();
      break;
      
    case MENU_HEATER_CTRL_PH:
      heaterMode = WBUS_PH;
      turn_on_heater();
      break;

    case MENU_HEATER_CTRL_SH:
      heaterMode = WBUS_SH;
      turn_on_heater(); 
      break;

    case MENU_HEATER_CTRL_OFF:
      turn_off_heater();
      break;
      
    case MENU_HEATER_CTRL_FP:
      wbus_fuelPrime(wbus, 30);
      break;

    case MENU_HEATER_SETMODE_VENT:
      settings.heaterMode = WBUS_VENT;
      flash_write(&fsettings, &settings, sizeof(settings_t));
      break;
      
    case MENU_HEATER_SETMODE_PH:
      settings.heaterMode = WBUS_PH;
      flash_write(&fsettings, &settings, sizeof(settings_t));
      break;

    case MENU_HEATER_SETMODE_SH:
      settings.heaterMode = WBUS_SH;
      flash_write(&fsettings, &settings, sizeof(settings_t));
      break;

    case MENU_HEATER_SENS_S0:
    case MENU_HEATER_SENS_S1:
    case MENU_HEATER_SENS_S2:
    case MENU_HEATER_SENS_S3:
    case MENU_HEATER_SENS_S4:
    case MENU_HEATER_SENS_S5:
    case MENU_HEATER_SENS_S6:
    case MENU_HEATER_SENS_S7:
    case MENU_HEATER_SENS_S8:
    case MENU_HEATER_SENS_S9:
    case MENU_HEATER_SENS_S10:
    case MENU_HEATER_SENS_S11:
    case MENU_HEATER_SENS_S12:
    case MENU_HEATER_SENS_S13:
    case MENU_HEATER_SENS_S14:
    case MENU_HEATER_SENS_S15:
    case MENU_HEATER_SENS_S16:
    case MENU_HEATER_SENS_S17:
    case MENU_HEATER_SENS_S18:
    case MENU_HEATER_SENS_S19:
      sprintf(digits, "%d", cmd-MENU_HEATER_SENS_S0+1);
      flags |= DISP_DIGITS;
      {
        wbus_sensor_read(wbus, &wbdata.wb_sensors, cmd-MENU_HEATER_SENS_S0);
        wbus_sensor_print(text, &wbdata.wb_sensors);
        flags |= DISP_TEXT;
      }
      break;

    default:
      break;			
  }
    
  if (wbus != NULL)
  {
    wbus_close(wbus);
    wbus = NULL;
  }
  
bail:

  if (err) {
    flags |= DISP_TEXT;
    strcpy(text, "Mist!");
    SET_ITER_NACK(flags)
  }

  *pflags = flags;
   
}

TASK_FUNC(openegg_wbus_thread)
{
  while (1) {
    if (fHeaterOn != 0)
    {
      HANDLE_WBUS wb;
      int error;
    
      error = wbus_open(&wb, WBUS_DEV);
      if (error == 0)
      {
        if (fHeaterOn == 2) {
          error = wbus_check(wb, heaterMode);
          if (error) {
            turn_off_heater();
          }
        } 
        if (fHeaterOn == 1) {
          error = wbus_turnOn(wb, heaterMode, settings.heatTime);
          if (error)
            turn_off_heater();
          else
            fHeaterOn = 2;
        }
        if (fHeaterOn < 0) {
          error = wbus_turnOff(wb);
          fHeaterOn = 0;
          machine_beep();
        }
        wbus_close(wb);
      } else {
        //strcpy(data, "Fehler");
      }
    }
    {
      int i;
      
      for (i=0; i<10; i++) {
        kernel_sleep(MSEC2JIFFIES(15000/10));
        kernel_yield();
        if (fHeaterOn == 1 || fHeaterOn == -1) {
          break;
        }
      }
    }
  }
}

static
void openegg_update_sym(void)
{
  ui_info sym;
  int ladedruck, sog;

  ladedruck = read_adc_and_convert(NULL,0);
  sog = read_adc_and_convert(NULL, 1);
  sym.olimex = 0;
  sym.sign = 0;
  /* Shifting 10 bit is not exactly dividing by 1000 but its much faster */
  if (ladedruck > 0)
    sym.bar = (ladedruck*13)>>10;
  else
    sym.bar = 0;
  if (sog > 0)
    sym.bat = (sog*5)>>10;
  else
    sym.bat = 0;  

  openegg_ui_displaySym(&sym);
}

TASK_FUNC(openegg_thread)
{
  int active = ACTIVE_TIMEOUT*50;

  while ( 1 ) {
    if ( machine_stayAwake() || machine_buttons(0) || fHeaterOn) {
      /* keep active > 0 but without disturbing the display delay below. */
      if ( (active & 7) == 0) {
        active = ACTIVE_TIMEOUT*50;
        machine_lcd(1);
      }
    }

    if (active > 0) {
      openegg_menu_state(openegg_do);
      openegg_update_sym();
      active--;
      if (active == 0) {
        machine_lcd(0);
      } else {
        if ( (active & 7) == 0 ) {
          openegg_ui_update();
        }
      }
    }
    kernel_sleep(MSEC2JIFFIES(20));
    kernel_yield();  
  }
}

TASK_FUNC(openegg_gsmctl_thread)
{
  gsmctl_open(&hgsmctl, GSM_DEV);
  gsmctl_registerCallback(hgsmctl, turn_on_heater_gsm, NULL);

  while (1) {
    gsmctl_thread(hgsmctl);
    kernel_sleep(MSEC2JIFFIES(1000));
    kernel_yield();
  }
}

void main(void)
{
  kernel_init();

  openegg_menu_init();

  memcpy(&settings, &fsettings, sizeof(settings_t));
    
  fHeaterOn = 0;
  fTimerOn = 0;
  ptime = &openegg_time;

  kernel_task_register(openegg_thread, KERNEL_STACK_SIZE/2);
  wbus_task = kernel_task_register(openegg_wbus_thread, KERNEL_STACK_SIZE/4); 
  kernel_task_register(openegg_gsmctl_thread, KERNEL_STACK_SIZE/4);
      
  kernel_run();
}
