/*
 * OpenEgg User Interface support. Currently PC simulation only.
 *
 * Author: Manuel Jander
 * License: BSD License
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#include "machine.h"

static struct termios oldios; 
static void _restore_term(void)
{
   tcsetattr(fileno(stdin), TCSANOW, &oldios);
}

/*
 * Init Display
 */
void openegg_ui_display_init(void)
{
   struct termios tios;
   
   tcgetattr(fileno(stdin), &oldios);
   tcgetattr(fileno(stdin), &tios);
   tios.c_lflag &= ~ICANON;
   tios.c_lflag &= ~ECHO;
   tios.c_cc[VMIN] = 1;
   tios.c_cc[VTIME] = 20;
   tcsetattr(fileno(stdin), TCSANOW, &tios);
   tcflush(fileno(stdin), TCIOFLUSH);
   atexit(_restore_term);
   printf("\n\n\n\n");
}


#define LINE_LEN 256
#define LINE_FILL 80
char screen[3][LINE_LEN] = { {0}, {0}, {0} };

void openegg_ui_update()
{
  printf("\033[3A%s\n%s\n%s\n", screen[0], screen[1], screen[2]);
}

static
void openegg_ui_fill(char *str)
{
  int len;
  
  len = strlen(str);
  if (len < LINE_FILL) {
    memset(str+len, ' ', LINE_FILL-1-len);
    str[LINE_FILL-1] = 0;
  }
}


/*
 * Display 2 strings (assumming not very big LCD display. May change,
 * depending what the current OpenEgg hardware actually can do.
 */
void openegg_ui_displaySym(const ui_info *uii)
{
  sprintf(screen[0], "bar: %d\tbat: %d", uii->bar, uii->bat);
  openegg_ui_fill(screen[0]);
}

void openegg_ui_displayText(const char *str1)
{
  sprintf(screen[2], "Text: %s", str1);
  openegg_ui_fill(screen[2]);
}

void openegg_ui_displayDigits(const char *str0)
{
  sprintf(screen[1], "Digits: %s", str0);
  openegg_ui_fill(screen[1]); 
}

/*
 * Read button bitmask
 */
unsigned char openegg_ui_btns(void)
{
  unsigned char btn = 0;
  char key = 0;

  key = machine_buttons(1);

  if (key == '1')
    btn = OPENEGG_BTN0;
    
  if (key == '2')
    btn = OPENEGG_BTN1;

  if (key == '3')
    btn = OPENEGG_BTN2;
     
  if (key == '4')
    btn = OPENEGG_BTN3;

  return btn;
}
