/*
 * OpenEgg User Interface support.
 *
 * Author: Manuel Jander
 * License: BSD License
 *
 */
#ifndef __OPENEGG_UI_H__
#define __OPENEGG_UI_H__

/* Work around backward compatibility. */
#ifdef __MSP430F149__ 
#define __MSP430_149__
#endif
#ifdef __MSP430F169__ 
#define __MSP430_169__
#endif
#ifdef __MSP430F1161__ 
#define __MSP430_1161__
#endif
#ifdef __MSP430F449__ 
#define __MSP430_449__
#endif


#define OPENEGG_BTN0	1
#define OPENEGG_BTN1	2
#define OPENEGG_BTN2	4
#define OPENEGG_BTN3	8

typedef struct
{
  unsigned char bar;	/* bar graph, 0 upto 10 */
  unsigned char bat;	/* battery indicator 0 upto 3 */

#if  0
  unsigned char arrows;	/* arrow bitmap */
  unsigned char units;	/* units */
#endif

  unsigned char sign;	/* +/- signs */
  unsigned char olimex;
} ui_info;

#if 0
#define ARROW_LEFT	0x10
#define ARROW_UP	0x20
#define	ARROW_RIGHT	0x40
#define ARROW_DOWN	0x80

#define UNITS_u		0x10
#define UNITS_m		0x20
#define UNITS_H		0x40
#define UNITS_F		0x80
#endif

#define SIGN_PLUS	0x80
#define SIGN_MINUS	0x10

#define OLIMEX		0x80

void openegg_ui_display_init(void);

/*
 * Display 2 strings (assumming not very big LCD display. May change,

 * depending what the current OpenEgg hardware actually can do.
 */
void openegg_ui_displayText(const char *str);
void openegg_ui_displayDigits(const char *str);
void openegg_ui_displaySym(const ui_info *ui);
void openegg_ui_update(void);

/*
 * Read button bitmask
 */
unsigned char openegg_ui_btns(void);

#endif /* __OPENEGG_UI_H__ */
