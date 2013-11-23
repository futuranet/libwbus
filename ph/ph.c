/*
 * PH Parking Heater translator for suplemental heaters
 * Author: M. Jander
 * License: BSD
 */

#include "machine.h"
#include "wbus.h"
#include "wbus_server.h"
#include "../wbus/wbus_const.h"
#include <stdio.h>
#include <string.h>


#ifdef __linux__
#include <stdlib.h>
#endif

static HANDLE_WBUS wc, wh;
static unsigned char cmd, cmd_heater, addr;
static int err, len;

#ifdef __linux__
/* Use rs232 ports defaults 1 and 2 for testing case. */
int PORT_TO_HEATER = 1;
int PORT_TO_EGG    = 2;
#else
/* Use first 2 ports for real hardware. */
#define PORT_TO_HEATER	0
#define PORT_TO_EGG	1
#endif

void main(int argc, char **argv)
{
#ifdef __linux__
  if (argc > 1) {
    PORT_TO_HEATER = atoi(argv[1]);
  }
  if (argc > 2) {
    PORT_TO_EGG = atoi(argv[2]);
  }
#endif

  err = wbus_open(&wc, PORT_TO_EGG);

#ifdef __linux__  
  if (err) {
    PRINTF("Error opening W-Bus handle\n");
    exit(-1);
  }
#endif

  while (1) {
    /* Listen to W-Bus message. Blocking I/O */
    err = wbus_host_listen(wc, &addr, &cmd, wbdata, &len);

    if (err == 0)
    {
      err = wbus_open(&wh, PORT_TO_HEATER);
      if (err != 0)  {
        PRINTF("Error opening port %d\n", PORT_TO_HEATER);
        continue;
      }
      /* Forward parking heater command as suplemental heater command. */
      if (cmd == WBUS_CMD_ON_PH) {
        cmd_heater = WBUS_CMD_ON_SH;
      } else {
        cmd_heater = cmd;
      }
      err = wbus_io(wh, cmd_heater, wbdata, NULL, 0, wbdata, &len, 0);
      if (err != 0)  {
        PRINTF("Error wbus_io() failed (%d)\n", err);
        continue;
      }      
      wbus_close(wh);
    }

    /* Sent W-Bus message response back to client. */
    err = wbus_host_answer(wc, addr, cmd, wbdata, len);
    if (err) {
      PRINTF("wbus_host_answer() failed\n");
    }
  }
}
