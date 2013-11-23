
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty_flip.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#define NO_PORTS 4

#define LP_INTERVAL msecs_to_jiffies(100)

#define dbg(arg...) { if (debug) { printk(arg); } }

/* Serial port pairs which will talk to its sibling */
static struct uart_port lp_port[NO_PORTS];
static unsigned int lp_mctrl[NO_PORTS];

static unsigned int c_iflags[NO_PORTS];
static unsigned int c_oflags[NO_PORTS];
static unsigned int c_cflags[NO_PORTS];
static unsigned int c_lflags[NO_PORTS];
static char c_cc[NO_PORTS][NCCS];

static unsigned int do_tx;
static unsigned int do_rx;
static struct timer_list lp_timer;
static spinlock_t timerlock;
static int do_echo = 0;
static int debug = 0;
static int log_data = 0;
module_param(do_echo, int, 0644);
MODULE_PARM_DESC(do_echo, "Echo mode enabled or not");  
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug mode enabled or not");  
module_param(log_data, int, 0644);
MODULE_PARM_DESC(log_data, "Log data mode enabled or not");  


void serial_loop_stop_tx(struct uart_port *port)
{
  dbg("Stop TX\n");
  
  spin_lock(&timerlock);
  do_tx &= ~(1<<port->line);
  spin_unlock(&timerlock);
}

void serial_loop_start_tx(struct uart_port *port)
{
  dbg("Start TX %d\n", port->line);

  spin_lock(&timerlock);
  do_tx |= (1<<port->line);
  spin_unlock(&timerlock);
}

static
void serial_loop_poll(int i)
{
  struct uart_port *xport, *rport;
  struct circ_buf *xmit = NULL;
  struct tty_struct *tty_recv = NULL;
  struct tty_struct *tty_xmit = NULL;
  int ii;

  int rxpush = 0;
  int echo_push = 0;
  int n_chars;

  if ( (do_tx & (1<<i)) == 0)
    return;

  dbg("polling port %d\n", i);

  ii = i^1;
  xport = &lp_port[i];
  rport = &lp_port[ii];
  
  if (xport != NULL) {
    if (xport->state != NULL) {
      xmit = &xport->state->xmit;
      tty_xmit = xport->state->port.tty;
    }
  }
  
  if (rport != NULL) {
    if (rport->state != NULL) {
      tty_recv = rport->state->port.tty;
    }
  }
  
  if (xmit == NULL)
    return;
  
  dbg("port %d tx empty %d, xchar %d, pending %d\n", xport->line, uart_circ_empty(xmit), xport->x_char, (int)uart_circ_chars_pending(xmit) );

  spin_lock(&timerlock);

  if (xport->x_char)
  {
    spin_lock(&timerlock);
  
    /* send port->x_char out the port here */
    if ( (do_rx & (1<<ii)) && tty_recv != NULL) {
      rport->icount.rx++;
      tty_insert_flip_char (tty_recv, xport->x_char, TTY_NORMAL);
      rxpush = 1;
    }
    
    if (do_echo && (do_rx & (1<<i)) ) {
      xport->icount.rx++;
      tty_insert_flip_char(tty_xmit, xport->x_char, TTY_NORMAL);
      echo_push = 1;
    }
    
    xport->icount.tx++;
    xport->x_char = 0;
    
    spin_unlock(&timerlock);
    return;
  }

  spin_unlock(&timerlock);

  if (uart_circ_empty(xmit)) {
    serial_loop_stop_tx(xport);
  }

  spin_lock(&timerlock);

  n_chars = uart_circ_chars_pending(xmit);
  
  if (n_chars > 0)
  {
    int n_chars0, n_chars1, a;
    
    n_chars0 = (UART_XMIT_SIZE - xmit->tail);
    if (n_chars0 > n_chars) {
      n_chars0 = n_chars;
    }
    n_chars1 = (xmit->tail > xmit->head) ? xmit->head : 0;

    if (log_data) {
      char string[256], *_s;
      static const char tb[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

      _s = string;
      for (a=0; a<n_chars0; a++)  {
        *_s++ = tb[(xmit->buf[xmit->tail + a] >> 4) & 0xf];
        *_s++ = tb[xmit->buf[xmit->tail + a] & 0xf];
        /*_s += sprintf(_s, "%02x ", ((int)xmit->buf[xmit->tail + a] & 0xff));*/
      }
      for (a=0; a<n_chars1; a++)  {
        *_s++ = tb[(xmit->buf[a] >> 4) & 0xf];
        *_s++ = tb[xmit->buf[a] & 0xf];
        /*_s += sprintf(_s, "%02x ", xmit->buf[a]);*/
      }
      *_s = 0;
      printk("TX port %d -> %d, %s\n", i, ii, string);
    }

    /* TX to counter part serial port */
    if ( (do_rx & (1<<ii)) && tty_recv != NULL ) {
      rport->icount.rx += n_chars;
      tty_insert_flip_string(tty_recv, xmit->buf + xmit->tail, n_chars0);
      tty_insert_flip_string(tty_recv, xmit->buf, n_chars1);
      rxpush = 1;
    }

    /* Do local echo if enabled */
    if (do_echo && xport != NULL && (do_rx & (1<<i)) ) {
      xport->icount.rx += n_chars;
      tty_insert_flip_string(tty_xmit, xmit->buf + xmit->tail, n_chars0);
      tty_insert_flip_string(tty_xmit, xmit->buf, n_chars1);
      echo_push = 1;
    }

    xmit->tail = (xmit->tail + n_chars) & (UART_XMIT_SIZE - 1);
    xport->icount.tx += n_chars;
  }

  spin_unlock(&timerlock);

  if ( n_chars && rxpush && (do_rx & (1<<ii))) {
    tty_flip_buffer_push(tty_recv);
  }
  
  if ( n_chars && echo_push && (do_rx & (1<<i)) ) {
    tty_flip_buffer_push(tty_xmit);
  }

  if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
    uart_write_wakeup(xport);
}


/* periodic poll for rx and tx */
static
void serial_loop_poll_timer(unsigned long data)
{
  int i;

  for (i=0; i<NO_PORTS; i++) {
    serial_loop_poll(i);
  }

  mod_timer(&lp_timer, jiffies + LP_INTERVAL);  
}


void serial_loop_send_xchar(struct uart_port *port, char ch)
{
  struct uart_port *rport;
  
  dbg("Send xchar\n");
  rport = &lp_port[port->line^1];
  rport->icount.rx++;
  tty_insert_flip_char (rport->state->port.tty, ch, 0);
  tty_schedule_flip (rport->state->port.tty);
}

void serial_loop_stop_rx(struct uart_port *port)
{
  dbg("Stop RX\n");
  do_rx &= ~(1<<port->line);
}

/*
 * \brief This function tests whether the transmitter fifo and shifter
 * for the port described by 'port' is empty.  If it is empty,
 * this function should return TIOCSER_TEMT, otherwise return 0.
 * If the port does not support this operation, then it should
 * return TIOCSER_TEMT.
 *  
 * Locking: none.
 * Interrupts: caller dependent.
 * This call must not sleep
 */
unsigned int serial_loop_tx_empty(struct uart_port *port)
{
  dbg("TX empty\n");
  return TIOCSER_TEMT;
}

void serial_loop_enable_ms(struct uart_port *port)
{

}

void serial_loop_break_ctl(struct uart_port *port,
                           int ctl)
{

}

void serial_loop_set_termios(struct uart_port *port,
                             struct ktermios *new,
                             struct ktermios *old)
{
  int i = port->line;

  dbg("Set termios\n");
  dbg("c_lflag = %x\n", new->c_lflag);
  dbg("echo = %d\n", new->c_lflag & ECHO);

  c_iflags[i] = new->c_iflag;
  c_oflags[i] = new->c_oflag;
  c_cflags[i] = new->c_cflag;
  c_lflags[i] = new->c_lflag;
  memcpy(c_cc[i], new->c_cc, sizeof(char)*NCCS);
}

int  serial_loop_startup(struct uart_port *port)
{
  dbg("Startup port\n");

  spin_lock(&timerlock);
  do_rx |= (1<<port->line);
  spin_unlock(&timerlock);

  return 0;
}

void serial_loop_shutdown(struct uart_port *port)
{
  dbg("Shutdown port\n");  

  spin_lock(&timerlock);
  do_rx &= ~(1<<port->line);
  do_tx &= ~(1<<port->line);
  spin_unlock(&timerlock);  
}

void serial_loop_change_speed(struct uart_port *port,
                              unsigned int cflag,
                              unsigned int iflag,
                              unsigned int quot)
{
  dbg("speed change\n");
}

/* pm(port,state,oldstate)
 * Perform any power management related activities on the specified
 * port.  State indicates the new state (defined by ACPI D0-D3),
 * oldstate indicates the previous state.  Essentially, D0 means
 * fully on, D3 means powered down.
 * 
 * This function should not be used to grab any resources.
 * 
 * This will be called when the port is initially opened and finally
 * closed, except when the port is also the system console.  This
 * will occur even if CONFIG_PM is not set.
 * 
 * Locking: none.
 * Interrupts: caller dependent.
 */
void serial_loop_pm(struct uart_port *port,
                    unsigned int state,
                    unsigned int oldstate)
{
  dbg("serial loop pm %d, state %d\n", port->line, state);

  switch (state) {
  case 0:
    /*serial_loop_start_tx(port);*/
    break;
  case 3:
    /*serial_loop_stop_tx(port);*/
    break;
  default:
    dbg(KERN_ERR "serial serial: unknown pm %d\n", state);
  }
}

int serial_loop_set_wake(struct uart_port *port,
                         unsigned int state)
{
  dbg("wake\n");
  return 0;
}



/*
 * get_mctrl(port)
 * Returns the current state of modem control inputs.  The state
 * of the outputs should not be returned, since the core keeps
 * track of their state.  The state information should include:
 *  - TIOCM_CD or TIOCM_CAR  state of DCD signal
 *  - TIOCM_CTS  state of CTS signal
 *  - TIOCM_DSR  state of DSR signal
 *  - TIOCM_RI  state of RI signal
 * The bit is set if the signal is currently driven active.  If
 * the port does not support CTS, DCD or DSR, the driver should
 * indicate that the signal is permanently active.  If RI is
 * not available, the signal should not be indicated as active.
 *  
 * Locking: port->lock taken.
 * Interrupts: locally disabled.
 * This call must not sleep
 */
static unsigned int serial_loop_get_mctrl(struct uart_port *port)
{
  dbg("Get mctrl 0x%08x\n", lp_mctrl[port->line]);
  return lp_mctrl[port->line];
}

/*
 * set_mctrl(port, mctrl)
 * This function sets the modem control lines for port described
 * by 'port' to the state described by mctrl.  The relevant bits
 * of mctrl are:
 *   - TIOCM_RTS  RTS signal.
 *   - TIOCM_DTR  DTR signal.
 *   - TIOCM_OUT1  OUT1 signal.
 *   - TIOCM_OUT2  OUT2 signal.
 *   - TIOCM_LOOP  Set the port into loopback mode.
 * If the appropriate bit is set, the signal should be driven
 * active.  If the bit is clear, the signal should be driven
 * inactive.
 * Locking: port->lock taken.
 * Interrupts: locally disabled.
 * This call must not sleep
 */
static void serial_loop_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
  dbg("Set mctrl 0x%08x\n", mctrl);
  lp_mctrl[port->line] = mctrl;
}


/*
 * Return a string describing the port type
 */
const char* serial_loop_type(struct uart_port *port)
{
  return "virtual loop back";
}

/*
 * Release IO and memory resources used by
 * the port. This includes iounmap if necessary.
 */
void serial_loop_release_port(struct uart_port *port)
{
  dbg("Release port\n");  
}

/*
 * Request IO and memory resources used by the
 * port.  This includes iomapping the port if
 * necessary.
 */
int  serial_loop_request_port(struct uart_port *port)
{
  dbg("Request port\n");
  return 0;
}

void serial_loop_config_port(struct uart_port *port, int config)
{
  dbg("Config\n");
}

int  serial_loop_verify_port(struct uart_port *port,
                             struct serial_struct *s)
{
  dbg("Verify\n");
  return 1;
}

int  serial_loop_ioctl(struct uart_port *port,
                       unsigned int ctl,
                       unsigned long data)
{
  struct termios *ptio = (struct termios *)data;
  int i = port->line;

  switch (ctl) {
    dbg("ioctl(%d , TCGETS, %d)\n", i, (int)data);
    case TCGETS:
      ptio->c_iflag = c_iflags[i];
      ptio->c_oflag = c_oflags[i];
      ptio->c_cflag = c_cflags[i];
      ptio->c_lflag = c_lflags[i];
      memcpy(ptio->c_cc, c_cc[i], NCCS*sizeof(char));
      break;
    case TCSETS:
      dbg("ioctl(%d , TCSETS, %d)\n", i, (int)data);
      c_iflags[i] = ptio->c_iflag;
      c_oflags[i] = ptio->c_oflag;
      c_cflags[i] = ptio->c_cflag;
      c_lflags[i] = ptio->c_lflag;
      memcpy(c_cc[i], c_cc, NCCS*sizeof(char));
      break;
    default:
      dbg("ioctl(%d ,%d, %d)\n", i, ctl, (int)data);
      break;
  }
  return 0;
}

/*
 * flush_buffer(port)
 * Flush any write buffers, reset any DMA state and stop any
 * ongoing DMA transfers.   
 *
 * This will be called whenever the port->state->xmit circular
 * buffer is cleared.
 *
 * Locking: port->lock taken.
 * Interrupts: locally disabled.
 * This call must not sleep
 */
void serial_loop_flush_port(struct uart_port *port)
{
  dbg("Flush buffer\n");
  serial_loop_poll(port->line);
}


static struct uart_ops serial_loop_ops = {
  .startup        = serial_loop_startup,
  .shutdown       = serial_loop_shutdown,
  .set_mctrl      = serial_loop_set_mctrl,
  .get_mctrl      = serial_loop_get_mctrl,
  .stop_tx        = serial_loop_stop_tx,
  .start_tx       = serial_loop_start_tx,
  .stop_rx        = serial_loop_stop_rx,
  .tx_empty       = serial_loop_tx_empty,
  .enable_ms      = serial_loop_enable_ms,
  .break_ctl      = serial_loop_break_ctl,
  .set_termios    = serial_loop_set_termios,
  .pm             = serial_loop_pm,
  .type           = serial_loop_type,
  .release_port   = serial_loop_release_port,
  .request_port   = serial_loop_request_port,
  .config_port    = serial_loop_config_port,
  .verify_port    = serial_loop_verify_port,
  .flush_buffer   = serial_loop_flush_port,
  /*.ioctl          = serial_loop_ioctl,*/
};


static struct uart_driver uart_drv = {
  .owner                  = THIS_MODULE,
  .driver_name            = "serial_loop",
  .dev_name               = "ttySL",
  .major                  = 0,
  .minor                  = 0,
  .nr                     = 8,
  .cons                   = NULL,
};

static int __init serial_loop_init(void)
{
  int retval, i;

  printk(KERN_INFO "Serial Loop init. Echo %d\n", (do_echo)?1:0);

  do_tx = 0;
  do_rx = 0;

  retval = uart_register_driver(&uart_drv);
  if (retval)
    return retval;

  for (i=0; i<NO_PORTS; i++) {
    c_cflags[i] = B2400 | CS8 | CLOCAL | CREAD;
    c_iflags[i] = IGNBRK | INPCK;
    c_oflags[i] = 0;
    c_lflags[i] = NOFLSH;
    c_cc[i][VTIME] = 10;
    c_cc[i][VMIN] = 0; 

    lp_mctrl[i] = TIOCM_CTS | TIOCM_DSR | TIOCM_CD;
    lp_port[i].line       = i;
    lp_port[i].ops        = &serial_loop_ops;
    lp_port[i].fifosize   = 1;              /* Not sure if this is required. */
    lp_port[i].type       = PORT_16550;      /* Make the picky serial_core happy */
    lp_port[i].membase    = (void*)lp_port; /* Make the picky serial_core happy */
    lp_port[i].iotype     = UPIO_MEM;       /* Make the picky serial_core happy */
    retval = uart_add_one_port(&uart_drv, &lp_port[i]);
    if (retval) {
      int x;

      for (x=0; x<i; x++) {
        uart_remove_one_port(&uart_drv, &lp_port[x]);
      }
      uart_unregister_driver(&uart_drv);
      return retval;
    }
  }
  
  /* Init timer for data polling */
  spin_lock_init(&timerlock); 
  init_timer(&lp_timer);
  lp_timer.function = serial_loop_poll_timer;
  lp_timer.expires = jiffies+LP_INTERVAL;
  lp_timer.data = 0;
  add_timer(&lp_timer);  

  printk(KERN_INFO "done\n");
  
  return 0;
}

static void __exit serial_loop_exit(void)
{
  int i;
  
  del_timer(&lp_timer);
  for (i=0; i<NO_PORTS; i++) {
    uart_remove_one_port(&uart_drv, &lp_port[i]);
  }

  uart_unregister_driver(&uart_drv);
  
  printk(KERN_INFO "Serial Loop unloaded\n");
}

module_init(serial_loop_init);
module_exit(serial_loop_exit);
MODULE_LICENSE("GPL");
