/*
 * RS232 portable access layer
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#include <stddef.h>
#include "machine.h"
#include "kernel.h"

/* delete or replace this with appropriate __attribute__(()) or whatever */
#define interrupt(x) void

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

int arm_uart_tx_isr(struct RS232 *rs232)
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
void arm_uart_tx_wait(struct RS232 *rs232)
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

int arm_uart_rx_isr(struct RS232 *rs232)
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

interrupt(USART0TX_VECTOR) arm_uart0tx_isr(void)
{
  int wup;
  
  wup = arm_uart_tx_isr(&uart0);

  if (wup) {
    machine_wakeup();
  }
}

interrupt(USART0RX_VECTOR) arm_uart0rx_isr(void)
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
    wup = arm_uart_rx_isr(&uart0);
  }

  if (wup) {
    machine_wakeup();
  }
}

interrupt(USART1TX_VECTOR) arm_uart1tx_isr(void)
{
  int wup;

  wup = arm_uart_tx_isr(&uart1);

  if (wup) {
    machine_wakeup();
  }
}

interrupt(USART1RX_VECTOR) arm_uart1rx_isr(void)
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
    wup = arm_uart_rx_isr(&uart1);
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
 
  /* Set data format specific register bits */
  switch (format) {
    case RS232_FMT_8N1:
      uctl = 0;  /* Start 8bit 1stop, No parity */
      break;
    case RS232_FMT_8E1:
      uctl = 0;  /* Start 8bit 1stop, Even parity */
      break;
    case RS232_FMT_8O1:
      uctl = 0;  /* Start 8bit 1stop, Odd parity */
      break;
    default:
      return -1;
  }

  /* Set BAUD specific pregister bits */
  switch (baudrate) {
    case 1200: ubr1=0x03; ubr0=0x69; mctl=0xFF; break;
    case 2400: ubr1=0x01; ubr0=0xB4; mctl=0xFF; break;
    case 4800: ubr1=0x00; ubr0=0xDA; mctl=0x55; break; 
    case 9600: ubr1=0x00; ubr0=0x6D; mctl=0x03; break;
    case 19200: ubr1=0x00; ubr0=0x36; mctl=0x6B; break;
    case 38400: ubr1=0x00; ubr0=0x1B; mctl=0x03; break;
    case 76800: ubr1=0x00; ubr0=0x0D; mctl=0x6B; break;
    case 115200: ubr1=0x00; ubr0=0x09; mctl=0x08; break;
    case 230400: ubr1=0x00; ubr0=0x15; mctl=0x92; break;
    default: return -1;
  }
                                                                  	 
  switch (dev_idx) {
  case 0:
    rs232 = &uart0;
    rs232->r2rs = 0;    
    rs232->regs1 = (pusart_regs1_t)0xbad;
    rs232->regs2 = (pusart_regs2_t)0xbad;

    /* ToDo Setup pins direction, peripheral etc for uart 0 */
    break;
  case 1:
    rs232 = &uart1;
    rs232->r2rs = 2;
    rs232->regs1 = (pusart_regs1_t)0xbad;
    rs232->regs2 = (pusart_regs2_t)0xbad;
    
    /* ToDo: Setup pins direction, peripheral etc for uart 1 */
    break;
  default:
    return -1;
  }
  
  /* ToDo: Copy bits to hardware registers */

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
  rs232->regs1->ctl |= 0xbad;
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
      while ( (rs232->regs1->tctl & 0xbad) == 0 ) ;
      arm_uart_tx_isr(rs232);
    }
    eint();
  }
}

void rs232_sbrk(HANDLE_RS232 rs232, unsigned char state)
{
   unsigned char pin;
   unsigned char *port_sel, *port_out;

   port_sel = (unsigned char *)0xbad;
   port_out = (unsigned char *)0xbad;
   pin = 0x40>>rs232->r2rs;

   /* ToDo: Set break condition using GPIO. There might be other methods */
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
  arm_uart_tx_wait(rs232);  
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
