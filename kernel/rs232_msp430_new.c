/*
 * RS232 portable access layer
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#include <io.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include "machine.h"
#include "kernel.h"

#define RX_BUF_SIZE 128
#define TX_BUF_SIZE 16

typedef struct {
  volatile unsigned char ctl;  /* UxCTL  */
  volatile unsigned char tctl; /* UxTCTL */
  volatile unsigned char rctl; /* UxRCTL */
  volatile unsigned char mctl; /* UxMCTL */
  volatile unsigned char br0;  /* UxBR0  */
  volatile unsigned char br1;  /* UxBR1  */
  volatile unsigned char rxbuf; /* UxRXBUF */
  volatile unsigned char txbuf; /* UxTXBUF */
} usart_regs1_t, *pusart_regs1_t;

typedef struct {
  volatile unsigned char ie;  /* IEx */
  unsigned char _filler0;
  volatile unsigned char ifg; /*  */
  unsigned char _filler1;
  volatile unsigned char me;  /* MEx */
} usart_regs2_t, *pusart_regs2_t;

static struct RS232
{
  pusart_regs1_t regs1;
  pusart_regs2_t regs2;
  unsigned char r2rs;                  /* regs2 right shift helper */

  unsigned int rx_timeout;             /* read timeout. */
  unsigned char block;                 /* do hard blocking or timeout on read (rx). */
  unsigned char task;                  /* id of the suspended task. */
  volatile unsigned char rx_waitbytes; /* how many bytes should be received until wake up. */
  volatile unsigned char tx_wait;      /* flag, indicating that tx thread is suspended. */

  /* Buffer handling */
  volatile unsigned char rx_cnt, rx_wr, rx_rd;
  unsigned char rxbuf[RX_BUF_SIZE];

  volatile unsigned char tx_cnt, tx_wr, tx_rd;
  unsigned char txbuf[TX_BUF_SIZE];
    
} uart0, uart1;

int msp430_uart_tx_isr(struct RS232 *rs232)
{
  if (rs232->tx_cnt > 0)
  {
    rs232->regs1->txbuf = rs232->txbuf[rs232->tx_rd];
    rs232->tx_rd = (rs232->tx_rd+1)&(TX_BUF_SIZE-1);
    rs232->tx_cnt--;
  }
  if (rs232->tx_cnt < TX_BUF_SIZE/2) {
    if (rs232->tx_wait) {
      rs232->tx_wait = 0;
      kernel_wakeup(rs232->task);
      return 1;
    }
  }
  return 0;
}

/* wait until we can tx */
static
void msp430_uart_tx_wait(struct RS232 *rs232)
{
  dint();
  if (rs232->tx_cnt > 0) {
    rs232->tx_wait = 1;
    rs232->task = kernel_getTask();
    kernel_suspend();
    eint();
    kernel_yield();
  }
  eint();
}

int msp430_uart_rx_isr(struct RS232 *rs232)
{
  unsigned char tmp;

  /* Read data (ack interrupt) */
  tmp = rs232->regs1->rxbuf;  

  if (rs232->rx_cnt < RX_BUF_SIZE)
  {
    rs232->rx_cnt++;
    rs232->rxbuf[rs232->rx_wr] = tmp;
    rs232->rx_wr = (rs232->rx_wr+1)&(RX_BUF_SIZE-1);    
  }

  /* Handle blocking */
  if (rs232->rx_waitbytes > 0) {
    rs232->rx_waitbytes--;
    if (rs232->rx_waitbytes == 0) {
      kernel_wakeup(rs232->task);
      return 1;
    }
  }

  return 0;
}

interrupt(USART0TX_VECTOR) msp430_uart0tx_isr(void)
{
  int wup;
  
  wup = msp430_uart_tx_isr(&uart0);

  if (wup) {
    machine_wakeup();
  }
}

interrupt(USART0RX_VECTOR) msp430_uart0rx_isr(void)
{
  int wup;

#if 0
  if ((IFG1 & URXIFG0) == 0) {
    /* This is a RX edge. Clear URXIFG0 and wakeup */
    U0TCTL &= ~URXSE;
    U0TCTL |= URXSE;
    wup=1;
  } else
#endif
  {
    wup = msp430_uart_rx_isr(&uart0);
  }

  if (wup) {
    machine_wakeup();
  }
}

interrupt(USART1TX_VECTOR) msp430_uart1tx_isr(void)
{
  int wup;

  wup = msp430_uart_tx_isr(&uart1);

  if (wup) {
    machine_wakeup();
  }
}

interrupt(USART1RX_VECTOR) msp430_uart1rx_isr(void)
{
  int wup=0;

#if 0
  if ((IFG2 & URXIFG1) == 0) {
    /* This is a RX edge. Clear URXIFG1 and wakeup */
    U1TCTL &= ~URXSE;
    U1TCTL |= URXSE;
    wup = 1;
  } else
#endif
  {
    wup = msp430_uart_rx_isr(&uart1);
  }

  if (wup) {
    machine_wakeup();
  }
}

int rs232_open(HANDLE_RS232 *pRs232, unsigned char dev_idx, long baudrate, unsigned char format)
{
  HANDLE_RS232 rs232 = NULL;
  unsigned char uctl = 0;
  unsigned int ubr1, ubr0, mctl;
 
  switch (format) {
    case RS232_FMT_8N1:
      uctl = CHAR;          /* Start 8bit 1stop, No parity */
      break;
    case RS232_FMT_8E1:
      uctl = CHAR|PENA|PEV; /* Start 8bit 1stop, Even parity */
      break;
    case RS232_FMT_8O1:
      uctl = CHAR|PENA;     /* Start 8bit 1stop, Odd parity */
      break;
    default:
      return -1;
  }

  switch (baudrate) {
#if defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__) /* BRCLK = ACLK = 32768 Hz */
    case 1200: ubr1=0x00; ubr0=0x1B; mctl=0x03; break;
    case 2400: ubr1=0x00; ubr0=0x0D; mctl=0x6B; break;
    case 4800: ubr1=0x00; ubr0=0x06; mctl=0x6F; break;
    case 9600: ubr1=0x00; ubr0=0x03; mctl=0x4A; break; /* Errate US14 !! */
#elif 0 /* BRCLK = SMCLK = 1.048.756 Hz */
    case 1200: ubr1=0x03; ubr0=0x69; mctl=0xFF; break;
    case 2400: ubr1=0x01; ubr0=0xB4; mctl=0xFF; break;
    case 4800: ubr1=0x00; ubr0=0xDA; mctl=0x55; break; 
    case 9600: ubr1=0x00; ubr0=0x6D; mctl=0x03; break;
    case 19200: ubr1=0x00; ubr0=0x36; mctl=0x6B; break;
    case 38400: ubr1=0x00; ubr0=0x1B; mctl=0x03; break;
    case 76800: ubr1=0x00; ubr0=0x0D; mctl=0x6B; break;
    case 115200: ubr1=0x00; ubr0=0x09; mctl=0x08; break;
#elif defined(__MSP430_449__) /* BRCLK = 4915200Hz */
    case 1200: ubr1=0x10; ubr0=0x00; mctl=0x00; break;
    case 2400: ubr1=0x08; ubr0=0x00; mctl=0x00; break;
    case 4800: ubr1=0x06; ubr0=0x00; mctl=0x00; break; 
    case 9600: ubr1=0x02; ubr0=0x00; mctl=0x00; break;
    case 19200: ubr1=0x01; ubr0=0x00; mctl=0x00; break;
    case 38400: ubr1=0x00; ubr0=0x80; mctl=0x00; break;
    case 56700: ubr1=0x00; ubr0=0x56; mctl=0xed; break;
    case 76800: ubr1=0x00; ubr0=0x40; mctl=0x00; break;
    case 115200: ubr1=0x00; ubr0=0x2a; mctl=0x6d; break;
    case 230400: ubr1=0x00; ubr0=0x15; mctl=0x92; break;
#elif 0 /* BRCLK = SMCLK = 8388608 Hz */
    case 1200: ubr1=0x1b; ubr0=0x4e; mctl=0x55; break;
    case 2400: ubr1=0x0d; ubr0=0xa7; mctl=0x22; break;
    case 4800: ubr1=0x06; ubr0=0xd3; mctl=0xad; break; 
    case 9600: ubr1=0x03; ubr0=0x69; mctl=0x7b; break;
    case 19200: ubr1=0x01; ubr0=0xb4; mctl=0xdf; break;
    case 38400: ubr1=0x00; ubr0=0xda; mctl=0xaa; break;
    case 56700: ubr1=0x00; ubr0=0x93; mctl=0xdf; break;
    case 76800: ubr1=0x00; ubr0=0x6d; mctl=0x44; break;
    case 115200: ubr1=0x00; ubr0=0x48; mctl=0x7b; break;
    case 230400: ubr1=0x00; ubr0=0x24; mctl=0x29; break;
#else
#error Not supported, unknown, do something.
#endif
    default: return -1;
  }
                                                                  	 
  switch (dev_idx) {
  case 0:
    rs232 = &uart0;
    rs232->r2rs = 0;    
    rs232->regs1 = (pusart_regs1_t)U0CTL_;
    rs232->regs2 = (pusart_regs2_t)0x00; /* IE1 */

#if defined(__MSP430_449__)
    P2SEL |= BIT4 | BIT5;      /* select UART pins */
    P2DIR |= BIT4;             /* port direction for TXD0 */
    P2DIR &= ~BIT5;            /* port direction for RXD0 */
#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
    P3SEL |= BIT6 | BIT7;      /* select UART pins */
    P3DIR |= BIT6;             /* port direction for TXD0 */
    P3DIR &= ~BIT7;            /* port direction for RXD0 */
#endif
    break;
  case 1:
    rs232 = &uart1;
    rs232->r2rs = 2;
    rs232->regs1 = (pusart_regs1_t)U1CTL_;
    rs232->regs2 = (pusart_regs2_t)0x01; /* IE2 */
    
#if defined(__MSP430_449__)    
    P4SEL |= BIT0 | BIT1;      /* select UART pins */
    P4DIR |= BIT0;             /* port direction for TXD1 */
    P4DIR &= ~BIT1;            /* port direction for RXD1 */
#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
    P3SEL |= BIT4 | BIT5;      /* select UART pins */
    P3DIR |= BIT4;             /* port direction for TXD1 */
    P3DIR &= ~BIT5;            /* port direction for RXD0 */ 
#endif
    break;
  default:
    return -1;
  }
  /* 1) Set SWRST */
  rs232->regs1->ctl = SWRST;
  /* 2) Initialize all USART registers with SWRST = 1 */
  rs232->regs1->ctl |= uctl;
#if defined(__MSP430_449__)
  rs232->regs1->tctl = SSEL2; /* BRCLK = SMCLK */
#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
  rs232->regs1->tctl = SSEL1; /* BRCLK = ACLK */
#else
#error Not supported, unknown, its your turn.
#endif
  rs232->regs1->rctl = 0;
  rs232->regs1->br0 = ubr0;
  rs232->regs1->br1 = ubr1;
  rs232->regs1->mctl = mctl;
  /* 3) Enable USART module via the MEx SFRs */  
  rs232->regs2->me |= 0xc0>>rs232->r2rs;
  uctl = rs232->regs1->rxbuf;
  /* 4) Clear SWRST via software */
  rs232->regs1->ctl &= ~SWRST;
  /* 5) Enable interrupts */
  rs232->regs2->ifg &= ~(0xc0>>rs232->r2rs);
  rs232->regs2->ie |= 0xc0>>rs232->r2rs;

  /* Init state */
  rs232->tx_cnt = 0;
  rs232->tx_wr = 0;
  rs232->tx_rd = 0;
  
  rs232->rx_cnt = 0;
  rs232->rx_wr = 0;
  rs232->rx_rd = 0;

  rs232->rx_waitbytes = 0;
  rs232->tx_wait = 0;
  rs232->block = 1;

  /* Return handle */  
  *pRs232 = rs232;

  return 0;
}

void rs232_close(HANDLE_RS232 rs232)
{
  rs232->regs2->ie &= ~(0xc0>>rs232->r2rs);
  rs232->regs1->ctl |= SWRST;
}

unsigned char rs232_read(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes)
{
  unsigned char n, i;
  
  if (nbytes > RX_BUF_SIZE) {
    nbytes = RX_BUF_SIZE;
  }

  /* Check if enough bytes are available */
  dint();
  if (rs232->rx_cnt < nbytes)
  {
    rs232->task = kernel_getTask();
    rs232->rx_waitbytes = nbytes-rs232->rx_cnt;
    if (rs232->block) {
      kernel_suspend();
    } else {
      kernel_sleep(0x4000 | (int)rs232->rx_waitbytes<<8); /* 32KHz jiffies / 240 Hz byte rate ~= 128, and factor 2 */
    }
    eint();
    kernel_yield();
  }
  eint();

  /* See how many bytes are available now */
  n = rs232->rx_cnt;
  if (n > nbytes) {
    n = nbytes;
  }
  
  for (i=0; i<n; i++)
  {
    *data++ = rs232->rxbuf[rs232->rx_rd];
    rs232->rx_rd = (rs232->rx_rd+1)&(RX_BUF_SIZE-1);
  }

  /* Release read bytes in buffer */
  dint();
  rs232->rx_cnt -= n;
  eint();

  return n;
}

int rs232_rxBytes(HANDLE_RS232 rs232)
{
  return rs232->rx_cnt;
}

void rs232_write(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes)
{
  int tmp, n, i;

  while (nbytes > 0)
  {
    /* If there is little room, wait until more bytes are transmitted. */
    dint();
    n = TX_BUF_SIZE - rs232->tx_cnt;
    if (n < nbytes && n < TX_BUF_SIZE/2)
    {
      rs232->task = kernel_getTask();
      rs232->tx_wait = 1;
      kernel_suspend();
      eint();
      kernel_yield();
      n = TX_BUF_SIZE - rs232->tx_cnt;
    }
    eint();

    if (nbytes < n ) {
      n = nbytes;
    }

    /* copy data into TX buffer */
    for (i=0; i<n; i++)
    { 
      rs232->txbuf[rs232->tx_wr] = *data++;
      rs232->tx_wr = (rs232->tx_wr+1)&(TX_BUF_SIZE-1);
    }
    
    nbytes -= n;

    /* update TX counters */
    dint();

    tmp = rs232->tx_cnt;
    rs232->tx_cnt += n;
    
    if (tmp == 0) {
      while ( (rs232->regs1->tctl & TXEPT) == 0 ) ;
      msp430_uart_tx_isr(rs232);
    }
    eint();
  }
}

void rs232_sbrk(HANDLE_RS232 rs232, unsigned char state)
{
   unsigned char pin;
   unsigned char *port_sel, *port_out;

#if defined(__MSP430_449__)
   if (rs232->r2rs == 0) {
     port_sel = (unsigned char *)P2SEL_;
     port_out = (unsigned char *)P2OUT_;
     pin = BIT4;
   } else {
     port_sel = (unsigned char *)P4SEL_;
     port_out = (unsigned char *)P4OUT_;
     pin = BIT0;
   }
#elif defined(__MSP430_169__) || defined(__MSP430_149__) || defined(__MSP430_1611__)
   port_sel = (unsigned char *)P3SEL_;
   port_out = (unsigned char *)P3OUT_;
   pin = 0x40>>rs232->r2rs;
#else
#error you loose.
#endif

   if (state) {
      *port_sel &= ~pin;
      *port_out &= ~pin;
   } else {
      *port_sel |= pin;
      *port_out |= pin;
   }
}

void rs232_flush(HANDLE_RS232 rs232)
{
  msp430_uart_tx_wait(rs232);  
  dint();
  rs232->tx_cnt = 0;
  rs232->tx_wr = 0;
  rs232->tx_rd = 0;
  rs232->rx_cnt = 0;
  rs232->rx_wr = 0;
  rs232->rx_rd = 0;
  eint();
}

void rs232_blocking(HANDLE_RS232 rs232, unsigned char block)
{
  rs232->block = block;
}
