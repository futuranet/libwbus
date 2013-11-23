/*
 * RS232 portable access layer
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

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


#ifdef __linux__
#define NO_RS232 8
#else
#define NO_RS232 2
#endif

typedef struct RS232 *HANDLE_RS232;

#define RS232_FMT_8N1 0
#define RS232_FMT_8E1 1
#define RS232_FMT_8O1 2

/**
 * \brief Open RS232 interface at given baud rate with 8N1 data format.
 * \param pRs232 pointer where a valid handle is written to.
 * \param dev_idx index of device to open.
 * \param baud baud rate.
 * \return error
 */
int rs232_open(HANDLE_RS232 *pRs232, unsigned char dev_idx, long baud, unsigned char format);

/**
 * \brief Close given RS232 handle
 */
void rs232_close(HANDLE_RS232 rs232);

/**
 * \brief Read data from RS232 port.
 * \param rs232 port handle
 * \param data Pointer where received data is written to.
 * \param nbytes Requested amount of bytes to read.
 * \return Amount of bytes recieved
 */
unsigned char rs232_read(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes);

/**
 * \brief Get amount of available bytes for reading
 * \param rs232 port handle
 * \return amount of available bytes
 */
int rs232_rxBytes(HANDLE_RS232 rs232);

/**
 * \brief Write data to RS232 port.
 * \param rs232 port handle
 * \param data Pointer where data to be transmitted is located.
 * \param nbytes byte count of data to be transmitted.
 */
void rs232_write(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes);

/**
 * \brief Set break status of RS232 port.
 * \param rs232 port handle
 * \param status If != 0 engage break status, If 0 release break status.
 */
void rs232_sbrk(HANDLE_RS232 rs232, unsigned char state);

/*
 * Empty all pending queues.
 */
void rs232_flush(HANDLE_RS232 rs232);

/**
 * \brief configure read blocking behaviour
 * \param rs232 port handle
 * \param block if 1 reads will block for ever until data arrives. If 0
 *              then the timeout is proportional to the amount of bytes to read. 
 */
void rs232_blocking(HANDLE_RS232 rs232, unsigned char block);
