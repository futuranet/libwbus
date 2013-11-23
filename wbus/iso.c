/* ************************************************************************* */
/*                Webasto communicatie routines op K-line interface          */
/*                comm. loopt op UART1                                       */
/*                2400 baud, 8 databit, 1 stopbit, no parity                 */
/*                21-01-2012                                                 */
/*								                                             */
/* ************************************************************************* */

#include "iso.h"
#include "rs232.h"
#include <ctype.h>

#define		BUFFLENGTH		20
#define		TARGET_ID		0x51
#define		MAXDATABYTES	16

typedef struct {
	char temperatuur;							/* bedrijfstemperatuur met 50 graden offset */
	int bedrijfsspanning;						/* klemspanning in millivolt, big endian */
	unsigned char vlamdetectie;					/* bit 0 = 1, kachel brand */
	unsigned char fuel_pump;
	unsigned char heater_activated;
	int kachelvermogen;							/* kachelvermogen in watt, big endian */
	int vlamdetectie_weerstand;					/* weerstand vlamdetector in milli ohm, big endian */
	unsigned char error_code;					/* foutcode */
	unsigned char no_of_error_codes;			/* aantal errorcodes aanwezig */
	unsigned char error_code_bytes[8];			/* div. foutcodes */
	unsigned char error_code_occurance[8];		/* div. foutcodes */
} HEATER_DATA;

struct iso_struct {
  HANDLE_RS232 rs232;
};

void iso_write(HANDLE_ISO iso, unsigned char format, unsigned char target, unsigned char source, unsigned char *data, unsigned int len)
{
  int index;
  unsigned char checksum;

  checksum = (format & 0xf0) | (len+4);
  rs232_write(iso->rs232, &checksum, 1);

  rs232_write(iso->rs232, &target, 1);
  rs232_write(iso->rs232, &source, 1);
  
  rs232_write(iso->rs232, data, len);

  checksum += target + source;
  for(index = 0; index < len; index++) {
    checksum += data[index];
  }
  rs232_write(iso->rs232, &checksum, 1);
}

int iso_read(HANDLE_ISO iso, unsigned char *format, unsigned char *target, unsigned char *source, unsigned char *data, unsigned int *plen)
{
  int err = 1, index;
  unsigned char tmp, tmp2;

  err = rs232_read(iso->rs232, &tmp, 1);
  if ( err != 1 || (tmp & 0xf0) != 0x80 ) {
    return -1;
  }

  tmp2 = (tmp & 7) - 2;
  err = rs232_read(iso->rs232, target, 1);
  if (err != 1) {
    return -1;
  }
  err = rs232_read(iso->rs232, source, 1);
  if (err != 1) {
    return -1;
  }
    
  err = rs232_read(iso->rs232, data, tmp2);
  if (err != tmp2) {
    return -1;
  }

  tmp = tmp + *target + *source;
  for(index = 0; index < tmp2; index++) {
    tmp += data[index];
  }

  err = rs232_read(iso->rs232, &tmp2, 1);
  if (err != 1 || tmp2 != tmp) {
    return -1;   
  }

  return 0;
}

int hextonumber(char hexchar) {
	hexchar = (char)toupper(hexchar);
	switch(hexchar) {
		case '0' :
		case '1' :
		case '2' :
		case '3' :
		case '4' :
		case '5' :
		case '6' :
		case '7' :
		case '8' :
		case '9' : 	return (hexchar - 0x30);
					break;
		case 'A' :
		case 'B' :
		case 'C' :
		case 'D' :
		case 'E' :
		case 'F' : 	return (hexchar - 0x37);
					break;
		default  :	break;
	}
	return 0;
}

int hextoi(char *hexstring) {
	int index, retval;
	index = 0;
	retval = 0;
	do {
		retval = retval*16 + hextonumber(hexstring[index]);
		index++;
	} while(hexstring[index]!= 0);
	return retval;
}

int get_heater_state(HEATER_DATA *kacheldata, unsigned char *combuff) {
	/* als het goed is komen er 10 bytes terug */
	/* byte0	: message header, 0x4f */
	/* byte1	: no. of bytes = 0x08 als het goed is */
	/* byte2	: Temperatur mit 50 Grad offset (20 Grad ist wert=70) */
	/* byte3,4	: Spannung in mili Volt, big endian */
	/* byte5	: Flame detector (set 0x01, not set 0x00) */
	/* byte6,7	: Heating power in watts, big endian */
    /* byte8,9	: Flame detector resistance in mili Ohm, big endian */
	/* byte10	: chacksum */
	kacheldata->temperatuur				= combuff[2] - 50;
	kacheldata->bedrijfsspanning		= (combuff[3] << 8) & 0xff00;
	kacheldata->bedrijfsspanning		|= combuff[4];
	kacheldata->vlamdetectie			= combuff[5];
	kacheldata->kachelvermogen			= (combuff[6] << 8) & 0xff00;
	kacheldata->kachelvermogen			|= combuff[7];
	kacheldata->vlamdetectie_weerstand	= (combuff[8] << 8) & 0xff00;
	kacheldata->vlamdetectie_weerstand	|= combuff[9];
	return 1;
}
