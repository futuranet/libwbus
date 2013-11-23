/*
 * Description: OpenEgg menu system
 * Author: Manuel Jander
 * License: BSD
 *
 */

#include "openegg_menu.h"

#include "machine.h"
#include <stdio.h>
#include <string.h>

/* Global variables */
static pmstate_t menu_curr;
static int menu_idx_curr;
//static int menu_iter;
static int state_sticky;
static int menu_flags;

static char uitext[128];
static char digits[7];

void openegg_menu_state(openegg_menu_callback cb)
{
  int id = MENU_NAV;
  const char *str=uitext;
  unsigned char btn;

  /* reset ITER_ACK */
  if (menu_flags & ITER_ACK)
    menu_flags &= ~ITER_ACK;   
  
  /* reset ITER_START */
  if (menu_flags & ITER_START) {
    menu_flags &= ~ITER_START;
    menu_flags |= ITER_IDLE;
  }

  btn = openegg_ui_btns();
        
  if (btn & (OPENEGG_BTN0|OPENEGG_BTN2))
  {        
    if (menu_flags & (ITER_NEXT|ITER_BACK|ITER_IDLE))
    {
      /* Select next or previous */
      if (btn & OPENEGG_BTN2) {
        menu_flags |= ITER_NEXT;
        menu_flags &= ~ITER_BACK;
      } else {
        menu_flags |= ITER_BACK;
        menu_flags &= ~ITER_NEXT;
      }
      id = menu_curr[menu_idx_curr].id & MENU_MASK;
    } else {
      if (btn & OPENEGG_BTN2) {
        if (menu_curr[menu_idx_curr].id & MENU_END)
          menu_idx_curr = 0;
        else
          menu_idx_curr++;
      } else {
        if (menu_idx_curr > 0)
          menu_idx_curr--;
        else
          while ((menu_curr[menu_idx_curr].id & MENU_END) == 0)
            menu_idx_curr++;
      }
      
      str = menu_curr[menu_idx_curr].text;
      menu_flags |= DISP_TEXT;
    }
  }

  if (btn & OPENEGG_BTN1)
  {
    if (menu_flags & (ITER_START|ITER_NEXT|ITER_BACK|ITER_IDLE)) {
      menu_flags &= ~(ITER_START|ITER_NEXT|ITER_BACK|ITER_IDLE);
      if (btn & OPENEGG_BTN1)
        menu_flags |= ITER_ACK;  /* Signal OK of iteration/counting state */
      else
        menu_flags |= ITER_NACK; /* Signal cancel, reject iteration/count result */
      
      str = menu_curr[menu_idx_curr].text;
      id = menu_curr[menu_idx_curr].id & MENU_MASK;
    } else {
      pmstate_t m_to_go = NULL;
      
      if (btn & OPENEGG_BTN1)
        m_to_go = menu_curr[menu_idx_curr].next;
      //else
      //  m_to_go = menu_curr[0].next;
      if (m_to_go != NULL) {
        /* Enter sub-menu */
        menu_curr = m_to_go;
        menu_idx_curr = 0;
        str = menu_curr[menu_idx_curr].text;
        menu_flags |= DISP_TEXT;
      } else {
        id = menu_curr[menu_idx_curr].id & MENU_MASK;
        /* Trigger iteration mode if required */
        if (menu_curr[menu_idx_curr].id & MENU_ITER)
          menu_flags |= ITER_START; 
      }
    }
  }

  if (btn & OPENEGG_BTN3) {
    machine_backlight_set(!machine_backlight_get());
  }     

  /* Process current state */
  if (id == MENU_NAV)
    id = state_sticky;
  
  if (id != MENU_NAV)
  {
    cb(id, &menu_flags, uitext, digits);
    if (menu_flags & CMD_STICKY)
      state_sticky = id;
    else
      state_sticky = MENU_NAV;
  }
  
  /* Do display handling */
  if (menu_flags & (DISP_TEXT|DISP_DIGITS))
  {     
    /* Update  display */
    if (menu_flags & DISP_TEXT) {
      openegg_ui_displayText(str);
    }

    if (menu_flags & DISP_DIGITS)
    {
      openegg_ui_displayDigits(digits);
      menu_flags &= ~DISP_DIGITS;
    }  
    menu_flags &= ~(DISP_TEXT);
  }
}


void openegg_menu_init(void)
{
  /* Init the display */
  openegg_ui_display_init();
  menu_flags = DISP_TEXT|DISP_DIGITS;
  *uitext = 0;
  *digits = 0;
  state_sticky = MENU_NAV;
  strcpy(uitext, "Servus!");

  /* Init states */
  menu_idx_curr = 0;
  menu_curr = menu;
}

