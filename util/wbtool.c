#include "wbus.h"
#include "machine.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

typedef enum {
	CMD_HELP = 1,
	CMD_ID,
	CMD_ERR_LS,
	CMD_ERR_DEL,
	CMD_TEST,
	CMD_PH, /* park heating */
	CMD_SH, /* supplemental heating */
	CMD_VENT, /* Ventilation */
	CMD_CHECK,
	CMD_STOP,
	CMD_MONITOR,
	CMD_MONITOR_SINGLE,
	CMD_EEPROM_RD,
	CMD_EEPROM_WR
} wbtool_cmd; 

int main(int argc, char **argv)
{
	HANDLE_WBUS wbus;
	wb_info_t i;
	wb_errors_t e;
	int err = 0;
	int test = 0, tim=1, dev = 0, sensor = 0; 
	float tval = 1.0f;
	wbtool_cmd cmd = CMD_HELP;
	char opt;
	char text[1024];
	unsigned char eeprom_data_wr[2];
	
	while ((opt = getopt(argc, argv, "ideEsPSVmcD:t:T:v:g:W:")) != -1)
	{
		switch (opt) {
		case 'i':
			cmd = CMD_ID;
			break;
		case 'd':
			cmd = CMD_ERR_DEL;
			break;
		case 'e':
			cmd = CMD_ERR_LS;
			break;
		case 's':
			cmd = CMD_STOP;
			break;
		case 'P':
			cmd = CMD_PH;
			break;
		case 'S':
		        cmd = CMD_SH;
		        break;
		case 'V':
		        cmd = CMD_VENT;
		        break;
		case 'c':
		        cmd = CMD_CHECK;
		        break;
		case 'm':
			cmd = CMD_MONITOR;
			break;
		case 'g':
			cmd = CMD_MONITOR_SINGLE;
			sensor =  atoi(optarg);
			break;
		case 'D':
			dev = atoi(optarg);
			break;
		case 't':
			cmd = CMD_TEST;
			test = atoi(optarg);
			break;
		case 'T':
			tim = atoi(optarg);
			break;
		case 'v':
			tval = atof(optarg);
			break;
                case 'E':
                        cmd = CMD_EEPROM_RD;
                        break;
                case 'W':
                        cmd = CMD_EEPROM_WR;
                        {
                          int tmp;
                          
                          tmp = atoi(optarg);
                          eeprom_data_wr[0] = (tmp>>8)&0xff;
                          eeprom_data_wr[1] = tmp & 0xff;
                        }
                        break;
		default:
			break;
		}
	}

	if (cmd == CMD_HELP)
	{
		printf("usage: %s [options]\n"
			" -i print info\n"
			" -E read and display EEPROM\n"
			" -W write EEPROM content\n"
			" -e scan error codes\n"
			" -d delete error codes\n"
			" -D serial port device\n"
			" -m scan sensors\n"
			" -g <i> read single sensor with index i \n"
			" -t n test subsystem n (1..15)\n"
			"   Known subsystems: CF=1 FP=2(freq) GP=3 CP=4 VF=5 SV=9 FPW=15 (CC=14)\n"
			"   -T tim Use time tim for the test\n"
			"   -v value Use value as Frequency or percent\n"
			" -s turn off heater\n"
			" -S turn on supplemental heater\n"
			" -P turn on park heater\n"
			" -V turn on ventilation\n"
			"Environment variable WBSERDEV is optional serial device default name\n", argv[0]);
		return 0;	 
	}

	/* Open comunication to W-Bus device */	
	wbus_open(&wbus, dev);
	if (err) {
	 	printf("Error opening W-Bus\n");
		return -1;
	}

	switch (cmd)
	{
		case CMD_ID:
			/* Retrieve device indenty info */
			err = wbus_get_wbinfo(wbus, &i);
			if (err == 0)
			{
			  int a;
			  
			  for (a=0; a<WB_IDENT_LINES; a++)
			  {
			    wbus_ident_print(text, &i, a);
			    printf("%s\n", text);
			  }
			} else {
			  printf("wbus_get_wbinfo() failed\n");
			}
			break;
		case CMD_EEPROM_RD:
			{
			  int a;
			  unsigned char eeprom_data[2];

			  for (a=0; a<32; a++)
			  {
			    err = wbus_eeprom_read(wbus, a, eeprom_data);
			    if (err == 0) {
			      printf("%x ", eeprom_data[a]);
			    } else {
			      break;
                            }
			  }
			}		        
		        break;
		case CMD_ERR_LS:
			/* read error codes  */
			wbus_errorcodes_read(wbus, &e);
			{
			  int a;
			  
			  for (a=0; a<e.nErr; a++)
			  {
			    wbus_errorcode_print(text, &e, a);
			    printf("%s\n", text);
			  }
			}
			break;

		case CMD_ERR_DEL:
			/* Delete error codes */
			err = wbus_errorcodes_clear(wbus);
			break;
		case CMD_TEST:
			printf("Testing device %d for %d seconds with value %f\n", test, tim, tval);
			err = wbus_test(wbus, test, tim, tval);
			break;
		case CMD_PH:
			err = wbus_turnOn(wbus, WBUS_PH, 30);
			break;
		case CMD_SH:
			err = wbus_turnOn(wbus, WBUS_SH, 30);
			break;
		case CMD_VENT:
			err = wbus_turnOn(wbus, WBUS_VENT, 30);
			break;
		case CMD_STOP:
			err = wbus_turnOff(wbus);
			break;
		case CMD_MONITOR:
			{
			int i;
			wb_sensor_t s;
			for (i=0; i<20; i++)
			{
			  wbus_sensor_read(wbus, &s, i);
			  wbus_sensor_print(text, &s);
			  printf("sensor[%d] %s\n", i, text);
			}
			}
			break;
		case CMD_MONITOR_SINGLE:
			{
			wb_sensor_t s;
			wbus_sensor_read(wbus, &s, sensor);
			wbus_sensor_print(text, &s);
			printf("sensor[%d] %s\n", sensor, text);
			}
			break;
		default:
			break;
			
	}

	while (!err) {
		switch (cmd) {
		case CMD_CHECK:
		case CMD_PH:
			err = wbus_check(wbus, WBUS_PH);
			break;
		case CMD_SH:
			err = wbus_check(wbus, WBUS_SH);
			break;
		case CMD_VENT:
			err = wbus_check(wbus, WBUS_VENT);
			break;
		default:
			err = -1;
			break;
		}
		if (err) {
		  printf("stop\n");
		  break;
		} 
		{
		  wb_sensor_t s;
		  wbus_sensor_read(wbus, &s, 3);
		  wbus_sensor_print(text, &s);
		  printf("%s\n", text);
		  wbus_sensor_read(wbus, &s, 5);
		  wbus_sensor_print(text, &s);
		  printf("%s\n", text);
		  wbus_sensor_read(wbus, &s, 7);
		  wbus_sensor_print(text, &s);
		  printf("%s\n", text);
		}
		printf(" running...\n");
		machine_sleep(1);
	}
	
	/* Close W-Bus */
	wbus_close(wbus);
	
	return err;
}
