/*
 * OpenEgg User Interface support. Currently PC simulation only.
 *
 * Author: Manuel Jander
 * License: BSD License
 *
 */

#include "machine.h"
#include <msp430.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __MSP430_449__
#define TEXT_LENGTH 7
#else
#define TEXT_LENGTH 16
#endif

/*
 * Read button bitmask
 */
unsigned char openegg_ui_btns(void)
{
  unsigned int cnt = 0;
  unsigned char btn = 0;
  static unsigned char prev = 0;
  static unsigned int wait,wcnt;
  unsigned char tmp;
  
  tmp = machine_buttons(1);
  
  /* No button pressed, so just leave. */
  if (tmp == 0) {
    prev = 0;
    wcnt = 0;
    return 0;
  }
  
  /* Control button repeat */
  if (wcnt > 0) {
    wcnt--;
    return 0;
  }

  /* Check for stable button signal (debounce). */
  while ( (machine_buttons(1) == tmp) && (cnt < 101))
    cnt++;

  /* Signal not steady, leave. */
  if ( cnt < 100 )
    return 0;

  /* Signal seems steady, decode buttons. */  
#ifdef __MSP430_449__
  if (tmp == 1) {
    btn = OPENEGG_BTN0;
  }
  if (tmp == 4) {
    btn = OPENEGG_BTN2;
  }
#else
  if (tmp == 1) {
    btn = OPENEGG_BTN2;
  }
  if (tmp == 4) {
    btn = OPENEGG_BTN0;
  }
#endif
  if (tmp == 2) {
    btn = OPENEGG_BTN1;
  }
  if (tmp == 8) {
    btn = OPENEGG_BTN3;
  }

  /* Wait until button returned to iddle or timeout */
  if (prev == btn) {
    wait = ((12*wait)>>4);
  } else {
#ifdef __MSP430_449__
    wait = 4;
#else
    wait = 20;
#endif
  }
  wcnt = wait;
  prev = btn;
  return btn;
}

static void openegg_ui_text_(char *str1);

static int length=0;
static int pos = 0;
static char text[128];

/* Display update callback */
void openegg_ui_update(void)
{
  static unsigned char delay_scroll=0;

  if (length > TEXT_LENGTH)
  {    
    if ( delay_scroll==0 ) {
      /* Do scrolling */
      pos++;
    }
    if (delay_scroll == 0) {
      delay_scroll = 2;
      if ( pos == 0 ) {
        delay_scroll = 4;
      }
    }
    delay_scroll--;
  }

  if (pos+TEXT_LENGTH+1 > length) {
    delay_scroll = 4;
    pos = -pos;
  }

  openegg_ui_text_(text + abs(pos));
}


/* Hight level text display with scrolling, buffering and friends. */
void openegg_ui_displayText(const char *str1)
{
  /* Get length of string on LCD */
  length = 0;
  {
    const char *ptr = str1;
    
    while (*ptr != 0) 
    {
      if (*str1 != '.' && *str1 != ':')
        length++;
      ptr++;
    }
  }
  
  pos = 0;
  
  if (length > TEXT_LENGTH)
  {
    int offset = length - TEXT_LENGTH;
    if (offset>2) offset=2;
    length += offset<<1;
    strcpy(text+offset, str1);
    memset(text, ' ', offset);
    memset(text+length-offset, ' ', offset);
  } else {
    strcpy(text, str1);
    if (length < TEXT_LENGTH)
      memset(text+length, ' ', TEXT_LENGTH-length);
  }
}

#if defined(__MSP430_449__)

/* MSP430 STK2 */

#define LED 8

/*
 * Init Display
 */
static unsigned char backlight_was_on=1;

void openegg_ui_display_init(void)
{
  /* Clear LCD Display */   
  for (i=0; i<20; i++) {
    LCDMEM[i]=0;   
  }

  LCDCTL = LCDON + LCD4MUX + LCDP0 + LCDP1 + LCDP2;       /* LCD 4Mux, S0-S39 */
  BTCTL = BTSSEL | BT_fCLK2_DIV2 | BT_fLCD_DIV64 | BTDIV; /* LCD freq ACLK/64 */
                                                          /* IRQ freq ACLK/256/2 */
  P5SEL = 0xFC;

  /* Enable or disable the LCD display only once. */
  if ( (LCDCTL & LCDON) == 0 ) {
    LCDCTL |= LCDON;  
    machine_backlight_set(backlight_was_on);
  }
}

void openegg_ui_display_off(void)
{
  if ( LCDCTL & LCDON ) {
    LCDCTL &= ~LCDON;
    backlight_was_on = machine_backlight_get();
    machine_backlight_set(0);
  }  
}

#define CHAR_MASK1 0x09
#define CHAR_MASK2 0x00

/* LCDMEM offset to text */
#define OFS_TEXT	6
#define OFS_TEXT_END	(OFS_TEXT+14)
/* LCDMEM offset to digits */
#define OFS_DGTS	2
#define OFS_DGTS_END	(OFS_DGTS+4)

static const unsigned short ascii_tab[128];

#define plus	0x8000
#define minus	0x1000

static const unsigned char digit_tab[27];


/* Small bit reverse helpers */
static unsigned char br_h(unsigned char x)
{
  unsigned char tmp;
  
  tmp = x & 0xf;
  tmp |= (x & 0x10)<<3;
  tmp |= (x & 0x20)<<1;
  tmp |= (x & 0x40)>>1;
  tmp |= (x & 0x80)>>3;
  return tmp;
}
static unsigned char br_l(unsigned char x)
{
  unsigned char tmp;
  
  tmp = x & 0xf0;
  tmp |= (x & 0x1)<<3;
  tmp |= (x & 0x2)<<1;
  tmp |= (x & 0x4)>>1;
  tmp |= (x & 0x8)>>3;
  return tmp;
}


void openegg_ui_displayDigits(const char *str0)
{
   int i;
   volatile char *plcd;
   unsigned char carry;
   
   carry = 0;
   
   /* Secondary 7 segment display */
   plcd = LCDMEM + OFS_DGTS_END-1;
   for  (i=0; i<7 && plcd>(LCDMEM+OFS_DGTS-1); i++)
   {
     unsigned char idx;
     unsigned short sym;
     
     idx  = str0[i] - '-';
     if (idx > 26)
       idx = 26;
     
     /* Write next character */
     sym = digit_tab[idx];
     
     sym |= carry;
     carry = 0;
     
     {
       unsigned char next;
       
       next = str0[i+1] - '-';
       
       if (next == 1)
       {
         if (plcd > LCDMEM+OFS_DGTS+1)
           sym |= digit_tab[next];
         else
           carry = digit_tab[next];
         i++;
       }
       if (next == 13)
       {
         if (plcd == LCDMEM+OFS_DGTS_END-2)
           carry = digit_tab[next];
         i++;
       }
     }
     
     /* Fixup 2nd and 3rd digit */
     if (plcd > LCDMEM+OFS_DGTS+1)
     {
       unsigned char tmp; 
       
       tmp = sym & 0x44;
       tmp |= (sym&0x01)?0x02:0x00;
       tmp |= (sym<<4)&0xa0;
       tmp |= (sym>>4)&0x09;
       tmp |= (sym&0x20)?0x10:0x00;
       
       sym = tmp;
     }
     
     *plcd-- = sym;
   }
}

/* Low level text display */
void openegg_ui_text_(char *str1)
{   
   int i;
   volatile char *plcd;

   /* Primary 14 segment display */
   plcd = LCDMEM + OFS_TEXT_END-2;
   for  (i=0; i<14 && plcd>(LCDMEM+OFS_TEXT-1); i++)
   {
     unsigned short tmp;
     
     tmp = ascii_tab[str1[i] & 0x7f];
     switch (str1[i]) {
       case '.':
       case ':':
       case ';':
         plcd[0] = tmp & 0xff;
         plcd[1] = (tmp>>8) & 0xff;
         i++;
         tmp = ascii_tab[str1[i] & 0x7f];
         break;
       default:
         plcd[0] = 0;
         plcd[1] = 0;
         break;
     }
     plcd[0] |= tmp & 0xff;
     plcd[1] |= (tmp>>8) & 0xff;
     plcd -= 2;
  }
}

void openegg_ui_displaySym(const ui_info *uii)
{
  /* extra symbols */
  LCDMEM[7] |= uii->olimex;
  LCDMEM[19] |= uii->sign;

#if  0

  /* Original hardware with broken bargraph */
  LCDMEM[1] = uii->arrows | uii->bat;
  LCDMEM[0] = uii->units | uii->bar;

#else

  /* Modified hardware with bargraph fix */
  {
    unsigned short tmp;
    unsigned char tmp0, tmp1;
    tmp = (2<<uii->bar)-1;
    tmp0 = tmp & 0xff;
    tmp1 = (tmp >> 4) & 0xf0;
    tmp1 |= br_l((1<<(uii->bat))-1) | 0x01; 
    LCDMEM[0] = br_l(tmp0);
    LCDMEM[1] =  br_h(tmp1);
  }

#endif

}

/* define segment bits */
#define a	0x0080
#define b 	0x0040
#define c	0x0020
#define d	0x0010
#define e	0x2000
#define f	0x4000
#define g	0x0400
#define h	0x0800
#define j	0x0008
#define k	0x0004
#define m	0x0002
#define n	0x0001
#define p	0x0100
#define q	0x0200
#define dp	0x1000
#define colon	0x8000

#define aa	0x10
#define bb	0x01
#define cc	0x04
#define dd	0x08
#define ee	0x40
#define	ff	0x20
#define gg	0x02
#define dpp	0x80
#define col	0x80

/* primary line. */
static const unsigned short ascii_tab[128] =
{
 0x0000, /*   */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /* \t */
 0x0000, /* \n */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*  */
 0x0000, /*   */
 0x0000, /* ! */
 f|j,		/* " */
 0x0000, /* # */
 0x0000, /* $ */
 f|g|h|k|q|m|n|c, /* % */
 0x0000, /* & */
 j,		/* ' */
 0x0000, /* ( */
 0x0000, /* ) */
 g|h|j|k|m|n|p|q, /* * */
 g|j|m|p,	/* + */
 dp,		/* , */
 g|m,		/* - */
 dp,		/* . */
 k|q,		/* / */
 a|b|c|d|e|f,	/* 0 */
 b|c,		/* 1 */
 a|b|g|m|d|e,	/* 2 */
 a|b|g|m|c|d,	/* 3 */
 b|c|g|m|f,	/* 4 */
 a|c|g|m|d|f,	/* 5 */
 a|c|g|m|d|e|f,	/* 6 */
 a|b|c,		/* 7 */
 a|b|c|g|m|d|e|f, /* 8 */
 a|b|c|g|m|d|f,	/* 9 */
 colon,		/* : */
 colon,		/* ; */
 k|n,		/* < */
 d|g|m,		/* = */
 h|q,		/* > */
 a|b|m|p,	/* ? */
 a|b|e|f|k|m,	/* @ */
 a|b|c|g|m|e|f,	/* A */
 a|b|c|d|j|m|p,	/* B */
 a|d|e|f,	/* C */
 a|b|c|d|j|p,	/* D */
 a|d|e|f|g,	/* E */
 a|e|f|g,	/* F */
 a|c|d|e|f|m,	/* G */
 b|c|e|f|g|m,	/* H */
 a|d|j|p,	/* I */
 b|c|d|e,	/* J */
 e|f|g|k|n,	/* K */
 d|e|f,		/* L */
 b|c|e|f|h|k,	/* M */
 b|c|e|f|h|n,	/* N */
 a|b|c|d|e|f,	/* O */
 a|b|e|f|g|m,	/* P */
 a|b|c|d|e|f|n,	/* Q */
 a|b|e|f|g|m|n,	/* R */
 a|c|d|f|g|m,	/* S */
 a|j|p,		/* T */
 b|c|d|e|f,	/* U */
 e|f|k|q,	/* V */
 b|c|e|f|n|q,	/* W */
 h|k|n|q,	/* X */
 h|k|p,		/* Y */
 a|d|k|q,	/* Z */
 a|d|j|p,	/* [ */
 h|n,		/* \ */
 a|d|j|p,	/* ] */
 q|n,		/* ^ */
 d, 		/* _ */
 q,		/* ` */
 d|e|g|n,	/* a */
 c|d|e|f|g|m,	/* b */
 d|e|g|m,	/* c */
 b|c|d|e|g|m, 	/* d */
 d|e|g|q,	/* e */
 a|j|m|p,	/* f */
 c|d|m|n,	/* g */
 c|e|f|g|m,	/* h */
 p,		/* i */
 p|d,		/* j */
 j|p|n|m,	/* k */
 d|j|p,		/* l */
 c|e|g|m|p,	/* m */
 c|g|m|p,	/* n */
 c|d|e|g|m,	/* o */
 e|f|g|h,	/* p */
 b|c|k|m,	/* q */
 m|p,		/* r */
 d|m|n,		/* s */
 g|j|m|p,	/* t */
 c|d|e,		/* u */
 e|q,		/* v */
 c|e|n|q,	/* w */
 g|m|n|q,	/* x */
 c|d|n,		/* y */
 d|g|q, 	/* z */
 g|j|p,		/* { */
 j|p,		/* | */
 j|m|p,		/* } */
 g|m,		/* ~ */
 a|b|m|p,	/*  */
};

/* digits, secondary line */
static const unsigned char digit_tab[27] =
{
  gg,			/* - */
  dpp,			/* . */
  bb|cc|dd,		/* / */
  aa|bb|cc|dd|ee|ff,	/* 0 */
  bb|cc,		/* 1 */
  aa|bb|dd|ee|gg,	/* 2 */
  aa|bb|cc|dd|gg,	/* 3 */
  bb|cc|gg|ff,		/* 4 */
  aa|cc|dd|ff|gg,	/* 5 */
  aa|cc|dd|ee|ff|gg,	/* 6 */
  aa|bb|cc,		/* 7 */
  aa|bb|cc|dd|ee|ff|gg,	/* 8 */
  aa|bb|cc|dd|ff|gg,	/* 9 */
  col,			/* : */
  col,			/* ; */
  0,			/* < */
  0,			/* = */
  0,			/* > */
  0,			/* ? */
  0,			/* @ */
  aa|bb|cc|dd|ee|ff,	/* A */
  cc|dd|ee|ff|gg,	/* B */
  aa|dd|ee|ff,		/* C */
  bb|cc|dd|ee|gg,	/* D */
  aa|dd|ee|ff|gg,	/* E */
  aa|ee|ff|gg,		/* F */
  0
};

#elif defined(__MSP430_149__) || defined(__MSP430_169__) || defined(__MSP430_1611__)

static unsigned char dogm162_readc(void)
{
  unsigned char a;

  P4DIR = 0;
  P5OUT |=  1; /* read */
  P5OUT &= ~4; /* command */
  P5OUT |=  2; /* enable */
  a = P4IN;
  P5OUT &= ~2; /* enable */

  return a;
}

static void dogm162_wait(void)
{
  /* Wait until busy flag is low again */
  while (dogm162_readc() & 0x80)
  {
#if 0
    kernel_sleep(1);
#else
    machine_usleep(30);
#endif
  }
}

static void dogm162_write(unsigned char a)
{
  dogm162_wait();

  P4OUT = a;
  P4DIR = 0xff;
  P5OUT &= ~1; /* write */
  P5OUT |=  4; /* data */
  P5OUT |=  2; /* enable */
  P5OUT &= ~2; /* enable */
}

static void dogm162_command_nowait(unsigned char c)
{
  P4OUT = c;
  P4DIR = 0xff;
  P5OUT &= ~1; /* write */
  P5OUT &= ~4; /* command */
  P5OUT |=  2; /* enable */
  P5OUT &= ~2; /* enable */
}

static void dogm162_command(unsigned char c)
{
  dogm162_wait();
  
  dogm162_command_nowait(c);
}

static int backlight_was_on = 0;
static unsigned char lcd_is_on = 0;

void openegg_ui_display_init(void)
{
  if (lcd_is_on) {
    return;
  }
  
  machine_msleep(40);
  dogm162_command_nowait(0x39); /* 8bits, 2 lines */
  machine_usleep(30);
  dogm162_command(0x14); /* bias */
  dogm162_command(0x55); /* enable booster */
  dogm162_command(0x6d); /* */
  dogm162_command(0x78); /* contrast */
  dogm162_command(0x38); 
  dogm162_command(0x0c); /* display on */
  dogm162_command(0x01); /* clear */
  dogm162_command(0x06); /* entry mode */

  machine_backlight_set(backlight_was_on);
  lcd_is_on = 1;
}

/* The Plan: use a DOGM16S2 display */
void openegg_ui_display_off(void)
{
  if (lcd_is_on) {
    backlight_was_on = machine_backlight_get();
    machine_backlight_set(0);
    dogm162_command(0x08); /* display off */
    lcd_is_on = 0;
  }
}

void openegg_ui_displayDigits(const char *str)
{
  int i;

  dogm162_command(0x80 | (16-5));
  for (i=0; i<5; i++) {
    dogm162_write(str[i]);
  }
}

void openegg_ui_displaySym(const ui_info *ui)
{
  /* not inplemented */
}

static void openegg_ui_text_(char *str1)
{
  int i;

  dogm162_command(0x80 | 0x40);
  for (i=0; i<TEXT_LENGTH; i++) {
    dogm162_write(str1[i]);
  }
}

#else /* Hardware selector */

#error Unsupported architecture

#endif /* Hardware selector */
