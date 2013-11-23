/*
 * RS232 portable access layer
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#include "kernel.h"
#include "machine.h"

#include <windows.h>
#include <signal.h>
#include <stdio.h>
#include <assert.h>

static
void printError(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    snprintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, (int)dw, (char*)lpMsgBuf); 

    //MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 
    printf("%s", (char*)lpDisplayBuf);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}


struct RS232
{
    HANDLE hSerial;
    HANDLE hSerialThread;
    int dev;
    unsigned char block;
    unsigned char task;
    volatile int rx_waitbytes;
};

static int rs232_bytes_avail(HANDLE_RS232 rs232)
{
  DWORD TotalBytesAvail;

#if 1
  if (!PeekNamedPipe(rs232->hSerial, NULL, 0, 0, &TotalBytesAvail, NULL)) {
    printError(TEXT("PeekNamedPipe"));
  }
#else
  COMSTAT cs;
  if (!ClearCommError(rs232->hSerial, NULL, &cs)) {
    printError(TEXT("ClearCommError"));
  }
  TotalBytesAvail = cs.cbInQue;
#endif

  return (int)TotalBytesAvail;
}

static
DWORD WINAPI rs232_isr(LPVOID lpParameter)
{
  HANDLE_RS232 rs232 = (HANDLE_RS232)lpParameter;
  DWORD dwEvtMask;

  while (1) 
  {    
    if (WaitCommEvent(rs232->hSerial, &dwEvtMask, NULL))
    {
      if (dwEvtMask & EV_RXCHAR) 
      {
        int repeat;

        do { 
          int bytes;

          repeat=0;

          bytes = rs232_bytes_avail(rs232);

          if ( rs232->rx_waitbytes <= bytes)
          {
            rs232->rx_waitbytes = 0;
            kernel_wakeup(rs232->task);
            SuspendThread(GetCurrentThread());
            repeat=1;
          }
        } while (repeat);
      }
    }
    else
    {
      DWORD dwRet = GetLastError();
      if( ERROR_IO_PENDING == dwRet)
      {
        printf("I/O is pending...\n");
           // To do.
      }
      else {
        printError(TEXT("WaitCommEvent"));
      }
    }
  }

  return 0;
}

static DWORD baud(long baudrate)
{
  DWORD baud;

  switch (baudrate) 
  {
    case 300: baud=CBR_300; break;
    case 600: baud=CBR_600; break;
    case 1200: baud=CBR_1200; break;
    case 2400: baud=CBR_2400; break;
    case 4800: baud=CBR_4800; break;
    case 9600: baud=CBR_9600; break;
    case 19200: baud=CBR_19200; break;
    case 38400: baud=CBR_38400; break;
    case 57600: baud=CBR_57600; break;
    case 115200: baud=CBR_115200; break;
    default: baud=CBR_115200; break;
  }
  return baud;
}

int rs232_open(HANDLE_RS232 *pRs232, unsigned char dev_idx, long baudrate, unsigned char format)
{
    HANDLE_RS232 rs232 = NULL;
    DCB dcbSerialParams = {0};
    char device[16];
    char *edp;

    rs232 = (HANDLE_RS232)malloc(sizeof(struct RS232));
    if (rs232 == NULL) {
       printError(TEXT("malloc"));
       goto bail;
    }

    sprintf(device, "WBSERDEV%d", dev_idx);
    edp = getenv(device);
    if (edp != NULL) {
      strcpy(device, edp);
    } else {
      edp = getenv("WBSERDEV");
      if (edp != NULL) {
        strcpy(device, edp);
      } else {
        strcpy(device, "COM1");
      }
      device[strlen(device)-1] = (char)('1' + dev_idx);
    }

    printf("RS232 device %s\n", device);

    rs232->hSerial = CreateFile(device,
                     GENERIC_READ | GENERIC_WRITE,
                     0,
                     0,
                     OPEN_EXISTING,
                     0,
                     NULL);

    if (rs232->hSerial == INVALID_HANDLE_VALUE) {
      printError(TEXT("CreateFile"));
      printf("Unable to open device %s\n", device);
      goto bail; 
    }
    rs232->dev = dev_idx;

    if (!GetCommState(rs232->hSerial, &dcbSerialParams)) {
      printError(TEXT("GetCommState"));
      goto bail;
    }

    switch (format) {
      case RS232_FMT_8E1:
        dcbSerialParams.fParity=TRUE;
        dcbSerialParams.Parity=EVENPARITY;
        break;
      case RS232_FMT_8O1:
        dcbSerialParams.fParity=TRUE;
        dcbSerialParams.Parity=ODDPARITY;
        break;
      default:
        dcbSerialParams.fParity=FALSE;
        break;
    }
    
    dcbSerialParams.fBinary=TRUE;
    dcbSerialParams.BaudRate=baud(baudrate);
    dcbSerialParams.ByteSize=8;
    dcbSerialParams.StopBits=ONESTOPBIT;
    dcbSerialParams.DCBlength = sizeof(DCB);
    if(!SetCommState(rs232->hSerial, &dcbSerialParams)){
      printError(TEXT("SetCommState"));
      goto bail;
    }

#if 1
    {
      COMMTIMEOUTS timeouts={0};

      if (!GetCommTimeouts(rs232->hSerial, &timeouts)){
        printError(TEXT("GetCommTimeouts"));
        goto bail;
      }
      timeouts.ReadIntervalTimeout=200;
      timeouts.ReadTotalTimeoutConstant=1000;
      timeouts.ReadTotalTimeoutMultiplier=50;
      timeouts.WriteTotalTimeoutConstant=1000;
      timeouts.WriteTotalTimeoutMultiplier=50;
      if(!SetCommTimeouts(rs232->hSerial, &timeouts)){
        printError(TEXT("GetCommTimeouts"));
        goto bail;
      }
    }

    if (! PurgeComm(rs232->hSerial, PURGE_RXABORT|PURGE_RXCLEAR|PURGE_TXABORT|PURGE_TXCLEAR) ) {
      printError(TEXT("PurgeComm"));
      goto bail;
    }
#endif

    rs232->hSerialThread = CreateThread(NULL, 0, rs232_isr, rs232, CREATE_SUSPENDED, 0 );

    if (!SetCommMask(rs232->hSerial, EV_RXCHAR)) {
      printError(TEXT("SetCommMask"));
      goto bail;
    }

    *pRs232 = rs232; 

    return 0;
    
bail:
    rs232_close(rs232);
    return -1;
}

void rs232_close(HANDLE_RS232 rs232)
{
    if (rs232 != NULL)
    {
        if (rs232->hSerial != INVALID_HANDLE_VALUE) {
             TerminateThread(rs232->hSerialThread, 0);
             CloseHandle(rs232->hSerialThread);
             CloseHandle(rs232->hSerial);
        }
        free (rs232);
    }    
}


unsigned char rs232_read(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes)
{
  int n, left;

  if (kernel_running()) {
    if (rs232_bytes_avail(rs232) < nbytes) {
      rs232->task = kernel_getTask();
      rs232->rx_waitbytes = nbytes;
      if (rs232->block) {
        kernel_suspend();
      } else {
        kernel_sleep( MSEC2JIFFIES(1000) | (int)rs232->rx_waitbytes<<4);
      }
      ResumeThread(rs232->hSerialThread);
      kernel_yield();
     /* SuspendThread(rs232->hSerialThread); */
    }
    left = rs232_bytes_avail(rs232);
    if (left > nbytes) {
      left = nbytes;
    }
  } else {
    left = nbytes;
  }

  //printf("nbytes = %d, left = %d\n", nbytes, left);

  left = nbytes;

  while (left > 0)
  {
    {
      DWORD nRead;

      if(!ReadFile(rs232->hSerial, data, left, &nRead, NULL)){
        printError(TEXT("ReadFile 3"));
      }
      n = nRead;
    }
    if (n <= 0) {
      break;
    }
    left -= n;
    data += n;
  }
  
  n = nbytes-left;

  //printf("read %d bytes\n", n);

  return n;
}

int rs232_rxBytes(HANDLE_RS232 rs232)
{
  return rs232_bytes_avail(rs232);
}

void rs232_write(HANDLE_RS232 rs232, unsigned char *data, unsigned char nbytes)
{
  DWORD nWritten;
 
  if(!WriteFile(rs232->hSerial, data, nbytes, &nWritten, 0)){
    printError(TEXT("WriteFile"));
  }

  if (nWritten < nbytes) {
    printf("WriteFile() did not write all bytes. This requires some rework here !\n");
  }
}

void rs232_sbrk(HANDLE_RS232 rs232, unsigned char state)
{
  if (state) {
    SetCommBreak(rs232->hSerial);
  } else {
    ClearCommBreak(rs232->hSerial);
  }
}

void rs232_flush(HANDLE_RS232 rs232)
{
  FlushFileBuffers(rs232->hSerial);

  if (! PurgeComm(rs232->hSerial, PURGE_RXABORT|PURGE_RXCLEAR|PURGE_TXABORT|PURGE_TXCLEAR) ) {
    printError(TEXT("PurgeComm 2"));
  }                
}

void rs232_blocking(HANDLE_RS232 rs232, unsigned char block)
{
  rs232->block = block;

  COMMTIMEOUTS timeouts={0};

  if (!GetCommTimeouts(rs232->hSerial, &timeouts)){
    printError(TEXT("GetCommTimeouts 2"));
    return;
  }
  if (block) {
    timeouts.ReadIntervalTimeout=0;
    timeouts.ReadTotalTimeoutConstant=0;
    timeouts.ReadTotalTimeoutMultiplier=0;
    timeouts.WriteTotalTimeoutConstant=0;
    timeouts.WriteTotalTimeoutMultiplier=0;
  } else {
    timeouts.ReadIntervalTimeout=200;
    timeouts.ReadTotalTimeoutConstant=1000;
    timeouts.ReadTotalTimeoutMultiplier=50;
    timeouts.WriteTotalTimeoutConstant=1000;
    timeouts.WriteTotalTimeoutMultiplier=50;
  }
  if(!SetCommTimeouts(rs232->hSerial, &timeouts)){
    printError(TEXT("GetCommTimeouts 2"));
  }
}
