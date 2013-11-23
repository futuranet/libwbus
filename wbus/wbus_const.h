/*
 * W-Bus constant definitions
 *
 * Author: Manuel Jander
 * mjander@users.sourceforge.net
 *
 */

#include <stdio.h>

/*
 * W-Bus addresses_
 * 0xf : thermo Test Software
 * 0x4 : heating device
 * 0x3 : 1533 Timer
 * 0x2 : Telestart
 */

/* address as client */
#define WBUS_CADDR 0xf
/* address as host */
#define WBUS_HADDR 0x4

/* W-Bus command refresh period in mili seconds */
#define CMD_REFRESH_PERIOD 20000L

/* W-Bus commands */
#define WBUS_CMD_OFF     0x10 /* no data */

#define WBUS_CMD_ON      0x20 /* For all CMD_ON_x : 1 byte = time in minutes */
#define WBUS_CMD_ON_PH   0x21 /* Parking Heating */
#define WBUS_CMD_ON_VENT 0x22 /* Ventilation */
#define WBUS_CMD_ON_SH   0x23 /* Supplemental Heating */
#define WBUS_CMD_CP      0x24 /* Circulation Pump external control */
#define WBUS_CMD_BOOST   0x25 /* Boost mode */

#define WBUS_CMD_SL_RD   0x32 /* Telestart system level read */
#define WBUS_CMD_SL_WR	 0x33 /* Telestart system level write */ 

/* discovered by pczepek, thank a lot ! */
#define WBUS_CMD_EEPROM_RD 0x35 /* READ_EEPROM [address]. 
                                 Response length 2 byte. Reads two bytes from
                                 eeprom memory, from address given as parameter.
                                 The eeprom is accessible from address 0x00 to 0x1f.
                                 In eeprom dump I found customer ID Number,
                                 registered remote fob codes, telestart system level etc. */
#define WBUS_CMD_EEPROM_WR 0x36 /* WRITE_EEPROM [address],[byte1],[byte2]
                                 Write two bytes to eeprom. Address, byte1, byte2
                                 as parameter. */

#define WBUS_CMD_U1      0x38 /* No idea. */
#define WBUS_TS_REGR     0x40 /* discovered by pczepek, Response length 0 byte.
                                 Parameter 0-0x0f. After issuing command,
                                 press remote fob OFF button to register.
                                 You can register max 3 remote fob. */

#define WBUS_CMD_FP	 0x42 /* Fuel prime. data 0x03 <2 bytes time in seconds / 2> */
#define WBUS_CMD_CHK     0x44 /* Check current command (0x20,0x21,0x22 or 0x23) */
#define WBUS_CMD_TEST    0x45 /* <1 byte sub system id> <1 byte time in seconds> <2 bytes value> */
#define WBUS_CMD_QUERY   0x50 /* Read operational information registers */
#define WBUS_CMD_IDENT   0x51 /* Read device identification information registers */
#define WBUS_CMD_OPINFO  0x53 /* 1 byte, operational info index. */
#define WBUS_CMD_ERR     0x56 /* Error related commands */
#define WBUS_CMD_CO2CAL  0x57 /* CO2 calibration */

#define WBUS_CMD_DATASET 0x58 /* (Not Webasto) data set related commands */

/* 0x50 Command parameters */
/* Status flags. Bitmasks below. STAxy_desc means status "x", byte offset "y", flag called 2desc" */
#define QUERY_STATUS0	0x02
#define 	STA00_SHR	0x10 /*!< Supplemental heater request*/
#define		STA00_MS	0x01 /*!< Main switch */
#define		STA01_S		0x01 /*!< Summer season */
#define		STA02_D		0x10 /*!< Generator signal D+ */
#define		STA03_BOOST	0x10 /*!< boost mode */
#define		STA03_AD	0x01 /*!< auxiliary drive */
#define		STA04_T15	0x01 /*!< ignition (terminal 15) */

#define QUERY_STATUS1	0x03
#define		STA10_CF	0x01 /*!< Combustion Fan	*/
#define		STA10_GP	0x02 /*!< GlÃ¼hkerze		*/
#define		STA10_FP	0x04 /*!< Fuel Pump             */
#define		STA10_CP	0x08 /*!< Circulation Pump	*/
#define		STA10_VF	0x10 /*!< Vehicle Fan Relay	*/
#define		STA10_NSH	0x20 /*!< Nozzle stock heating  */
#define		STA10_FI	0x40 /*!< Flame indicator       */
       
#define QUERY_OPINFO0		0x04 /* Fuel type, max heat time and factor for shortening ventilation time (but details are unclear) */
#define         OP0_FUEL        0      /*!< 0x0b: gasoline, 0x0d: diesel, 0:neutral */
#define         OP0_TIME        1      /*!< max heating time / 10 in minutes */
#define         OP0_FACT        2      /*!< ventilation shortening factor (?) */

#define QUERY_SENSORS	0x05 /*!< Assorted sensors. 8 bytes. Byte offsets below. */
#define		SEN_TEMP	0	/*!< Temperature with 50°C offset (20°C is represented by 70) */
#define		SEN_VOLT	1	/*!< 2 bytes Spannung in mili Volt, big endian */
#define		SEN_FD		3	/*!< 1 byte Flame detector flag */
#define		SEN_HE		4	/*!< 2 byte, heating power, in percent or watts (semantic seems wrong, heating energy?) */
#define		SEN_GPR		6	/*!< 2 byte, glow plug resistance in mili Ohm. */

#define QUERY_COUNTERS1	0x06
#define		WORK_HOURS	0 /*!< Working hours     */
#define		WORK_MIN	2 /*!< Working minutes   */
#define		OP_HOURS	3 /*!< Operating hours   */
#define		OP_MIN		5 /*!< Operating minutes */
#define		CNT_START	6 /*!< Start counter     */

#define QUERY_STATE	0x07
#define		OP_STATE	0
#define		OP_STATE_N	1
#define		DEV_STATE	2
/* 3 more unknown bytes */
#define WB_STATE_BO	0x00 /* Burn out */
#define	WB_STATE_DEACT1	0x01 /* Deactivation */
#define	WB_STATE_BOADR	0x02 /* Burn out ADR (has something to due with hazardous substances transportation) */
#define	WB_STATE_BORAMP	0x03 /* Burn out Ramp */
#define	WB_STATE_OFF	0x04 /* Off state */
#define	WB_STATE_CPL	0x05 /* Combustion process part load */
#define	WB_STATE_CFL	0x06 /* Combustion process full load */
#define	WB_STATE_FS	0x07 /* Fuel supply */
#define	WB_STATE_CAFS	0x08 /* Combustion air fan start */
#define	WB_STATE_FSI	0x09 /* Fuel supply interruption */
#define	WB_STATE_DIAG	0x0a /* Diagnostic state */
#define	WB_STATE_FPI	0x0b /* Fuel pump interruption */
#define	WB_STATE_EMF	0x0c /* EMF measurement */
#define	WB_STATE_DEB	0x0d /* Debounce */
#define	WB_STATE_DEACTE	0x0e /* Deactivation */
#define	WB_STATE_FDI	0x0f /* Flame detector interrogation */
#define	WB_STATE_FDC	0x10 /* Flame detector cooling */
#define	WB_STATE_FDM	0x11 /* Flame detector measuring phase */
#define	WB_STATE_FDMZ	0x12 /* Flame detector measuring phase ZUE */
#define	WB_STATE_FAN	0x13 /* Fan start up */
#define	WB_STATE_GPRAMP	0x14 /* Glow plug ramp */
#define	WB_STATE_LOCK	0x15 /* Heater interlock */
#define	WB_STATE_INIT	0x16 /* Initialization   */
#define	WB_STATE_BUBLE	0x17 /* Fuel bubble compensation */
#define	WB_STATE_FANC	0x18 /* Fan cold start-up */
#define	WB_STATE_COLDR	0x19 /* Cold start enrichment */
#define	WB_STATE_COOL	0x1a /* Cooling */
#define	WB_STATE_LCHGUP	0x1b /* Load change PL-FL */
#define	WB_STATE_VENT	0x1c /* Ventilation */
#define	WB_STATE_LCHGDN	0x1d /* Load change FL-PL */
#define	WB_STATE_NINIT	0x1e /* New initialization */
#define	WB_STATE_CTRL	0x1f /* Controlled operation */
#define	WB_STATE_CIDDLE	0x20 /* Control iddle period */
#define	WB_STATE_SSTART	0x21 /* Soft start */
#define	WB_STATE_STIME	0x22 /* Savety time */
#define	WB_STATE_PURGE	0x23 /* Purge */
#define	WB_STATE_START	0x24 /* Start */
#define	WB_STATE_STAB	0x25 /* Stabilization */
#define	WB_STATE_SRAMP	0x26 /* Start ramp    */
#define	WB_STATE_OOP	0x27 /* Out of power  */
#define	WB_STATE_LOCK2	0x28 /* Interlock     */
#define WB_STATE_LOCKADR	0x29 /* Interlock ADR (Australian design rules) */
#define	WB_STATE_STABT	0x2a /* Stabilization time */
#define	WB_STATE_CHGCTRL	0x2b /* Change to controlled operation */
#define	WB_STATE_DECIS	0x2c /* Decision state */
#define	WB_STATE_PSFS	0x2d /* Prestart fuel supply */
#define	WB_STATE_GLOW	0x2e /* Glowing */
#define	WB_STATE_GLOWP	0x2f /* Glowing power control */
#define	WB_STATE_DELAY	0x30 /* Delay lowering */
#define	WB_STATE_SLUG	0x31 /* Sluggish fan start */
#define	WB_STATE_AGLOW	0x32 /* Additional glowing */
#define	WB_STATE_IGNI	0x33 /* Ignition interruption */
#define	WB_STATE_IGN	0x34 /* Ignition */
#define	WB_STATE_IGNII	0x35 /* Intermittent glowing */
#define	WB_STATE_APMON	0x36 /* Application monitoring */
#define	WB_STATE_LOCKS	0x37 /* Interlock save to memory */
#define	WB_STATE_LOCKD	0x38 /* Heater interlock deactivation */
#define	WB_STATE_OUTCTL	0x39 /* Output control */
#define	WB_STATE_CPCTL	0x3a /* Circulating pump control */
#define	WB_STATE_INITUC	0x3b /* Initialization uP */
#define	WB_STATE_SLINT	0x3c /* Stray light interrogation */
#define	WB_STATE_PRES	0x3d /* Prestart */
#define	WB_STATE_PREIGN	0x3e /* Pre-ignition */
#define	WB_STATE_FIGN	0x3f /* Flame ignition */
#define	WB_STATE_FSTAB	0x40 /* Flame stabilization */
#define	WB_STATE_PH	0x41 /* Combustion process parking heating */
#define	WB_STATE_SH	0x42 /* Combustion process suppl. heating  */
#define	WB_STATE_PHFAIL	0x43 /* Combustion failure failure heating */
#define	WB_STATE_SHFAIL	0x44 /* Combustion failure suppl. heating  */
#define	WB_STATE_OFFR	0x45 /* Heater off after run */
#define	WB_STATE_CID	0x46 /* Control iddle after run */
#define	WB_STATE_ARFAIL	0x47 /* After-run due to failure */
#define	WB_STATE_ARTCTL	0x48 /* Time-controlled after-run due to failure */
#define	WB_STATE_LOCKCP	0x49 /* Interlock circulation pump */
#define	WB_STATE_CIDPH	0x4a /* Control iddle after parking heating */
#define	WB_STATE_CIDSH	0x4b /* Control iddle after suppl. heating  */
#define	WB_STATE_CIDHCP	0x4c /* Control iddle period suppl. heating with circulation pump */
#define	WB_STATE_CPNOH	0x4d /* Circulation pump without heating function */
#define	WB_STATE_OV	0x4e /* Waiting loop overvoltage */
#define	WB_STATE_MFAULT	0x4f /* Fault memory update */
#define	WB_STATE_WLOOP	0x50 /* Waiting loop */
#define	WB_STATE_CTEST	0x51 /* Component test */
#define	WB_STATE_BOOST	0x52 /* Boost */
#define	WB_STATE_COOL2	0x53 /* Cooling */
#define	WB_STATE_LOCKP	0x54 /* Heater interlock permanent */
#define	WB_STATE_FANIDL	0x55 /* Fan iddle */
#define	WB_STATE_BA	0x56 /* Break away */
#define	WB_STATE_TINT	0x57 /* Temperature interrogation */
#define	WB_STATE_PREUV	0x58 /* Prestart undervoltage */
#define	WB_STATE_AINT	0x59 /* Accident interrogation */
#define	WB_STATE_ARSV	0x5a /* After-run solenoid valve */
#define	WB_STATE_MFLTSV	0x5b /* Fault memory update solenoid valve */
#define	WB_STATE_TCARSV	0x5c /* Timer-controlled after-run solenoid valve */
#define	WB_STATE_SA	0x5d /* Startup attempt */
#define	WB_STATE_PREEXT	0x5e /* Prestart extension */
#define	WB_STATE_COMBP	0x5f /* Combustion process */
#define	WB_STATE_TIARUV	0x60 /* Timer-controlled after-run due to undervoltage */
#define	WB_STATE_MFLTSW	0x61 /* Fault memory update prior switch off */
#define	WB_STATE_RAMPFL	0x62 /* Ramp full load */
    /*byte1 Operating state state number*/
    /*byte2 Device state*/
#define	WB_DSTATE_STFL	0x01 /* STFL */
#define	WB_DSTATE_UEHFL	0x02 /* UEHFL */
#define	WB_DSTATE_SAFL	0x04 /* SAFL   */
#define	WB_DSTATE_RZFL	0x08 /* RZFL */
    /*byte3,4,5: Unknown*/

#define QUERY_DURATIONS0 0x0a /* 24 bytes */

#define QUERY_DURATIONS1 0x0b /* 6 bytes*/
#define		DUR1_PH	0 /* Parking heating duration, hh:m */
#define		DUR1_SH 3 /* Supplemental heating duration hh:m */

#define QUERY_COUNTERS2	0x0c
#define		STA3_SCPH	0	/*!< 2 bytes, parking heater start counter  */
#define		STA3_SCSH	2	/*!< 2 bytes, supplemtal heater start counter */
#define		STA34_FD	0x00	/*!< Flame detected  */

#define QUERY_STATUS2	0x0f
#define		STA2_GP		0 /* glow plug (ignition/flame detection)*/
#define		STA2_FP		1 /* fuel pump */
#define		STA2_CAF	2 /* combustion air fan */
#define		STA2_U0		3 /* unknown */
#define		STA2_CP		4 /* (coolant) circulation pump */

#define QUERY_OPINFO1 0x11
#define		OP1_THI 0 /* Lower temperature threshold */
#define		OP1_TLO 1 /* Higher temperature threshold */
#define         OP1_U0  2

#define QUERY_DURATIONS2 0x12 /* 3 bytes */
#define		DUR2_VENT 0 /* Ventilation duration hh:m */
                    
#define	QUERY_FPW	0x13 /*!< Fuel prewarming. May not be available. See wbcode */
#define		FPW_R		0	/*!< 2 bytes: Current fuel prewarming PTC resistance in mili ohm, big endian */
#define		FPW_P		2	/*!< 2 bytes: Currently applied fuel prewarming power in watts, big endian */


/* 0x51 Command parameters */
#define IDENT_DEV_ID		0x01 /*!< Device ID Number */
#define IDENT_HWSW_VER		0x02 /*!< Hardware version (KW/Jahr), Software version, Software version EEPROM, 6 bytes */
#define IDENT_DATA_SET		0x03 /*!< Data Set ID Number */
#define IDENT_DOM_CU		0x04 /*!< Control Unit Herstellungsdatum (Tag monat jahr je ein byte) */
#define IDENT_DOM_HT		0x05 /*!< Heizer Herstellungsdatum (Tag monat jahr je ein byte) */
#define IDENT_TSCODE		0x06 /*!< Telestart code */
#define IDENT_CUSTID		0x07 /*!< Customer ID Number (Die VW Teilenummer als string und noch ein paar Nummern dran) + test sig */
#define IDENT_U0                0x08 /*!< ? */
#define IDENT_SERIAL		0x09 /*!< Serial Number */
#define IDENT_WB_VER		0x0a /*!< W-BUS version. Antwort ergibt ein byte. Jedes nibble dieses byte entspricht einer Zahl (Zahl1.Zahl2) */
#define IDENT_DEV_NAME		0x0b /*!< Device Name: Als character string zu interpretieren. */
#define IDENT_WB_CODE		0x0c /*!< W-BUS code. 7 bytes. This is sort of a capability bit field */
/* W-Bus code bits */
#define     WB_CODE_0     0 /* Unknown supplemental heater feature */
#define     WB_CODE_ON    3 /* on/off switch capability */
#define     WB_CODE_PH    4 /* Parking heater capability */
#define     WB_CODE_SH    5 /* Supplemental heater capability */
#define     WB_CODE_VENT  6 /* Ventilation capability */
#define     WB_CODE_BOOST 7 /* Boost capability */

#define     WB_CODE_ECPC  9 /* External circulation pump control */
#define     WB_CODE_CAV  10 /* Combustion air fan (CAV) */
#define     WB_CODE_GP   11 /* Glow Plug (flame detector) */
#define     WB_CODE_FP   12 /* Fuel pump (FP) */
#define     WB_CODE_CP   13 /* Circulation pump (CP) */
#define     WB_CODE_VFR  14 /* Vehicle fan relay (VFR) */
#define     WB_CODE_LEDY 15 /* Yellow LED */

#define     WB_CODE_LEDG 16 /* Green LED present */
#define     WB_CODE_ST   17 /* Spark transmitter. Implies no Glow plug and thus no resistive flame detection */
#define     WB_CODE_SV   18 /* Solenoid valve present (coolant circuit switching) */
#define     WB_CODE_DI   19 /* Auxiliary drive indicator (whatever that means) */
#define     WB_CODE_D    20 /* Generator signal D+ present */
#define     WB_CODE_CAVR 21 /* Combustion air fan level is in RPM instead of percent */
#define     WB_CODE_22   22 /* (ZH) */
#define     WB_CODE_23   23 /* (ZH) */

#define     WB_CODE_CO2  25 /* CO2 calibration */
#define     WB_CODE_OI   27 /* Operation indicator (OI) */

#define     WB_CODE_32   32 /* (ZH) */
#define     WB_CODE_33   33 /* (ZH) */
#define     WB_CODE_34   34 /* (ZH) */
#define     WB_CODE_35   35 /* (ZH) */
#define     WB_CODE_HEW  36 /* Heating energy is in watts (or if not set in percent and the value field must be divided
                               by 2 to get the percent value) */
#define     WB_CODE_37   37 /* (ZH) */
#define     WB_CODE_FI   38 /* Flame indicator (FI) */
#define     WB_CODE_NSH  39 /* Nozzle Stock heating (NSH) */

#define     WB_CODE_T15  45 /* Ignition (T15) flag present */
#define     WB_CODE_TTH  46 /* Temperature thresholds available, command 0x50 index 0x11 */
#define     WB_CODE_VPWR 47 /* Fuel prewarming resistance and power can be read. */

#define     WB_CODE_SET  57 /* 0x02 Set value flame detector resistance (FW-SET),
                               set value combustion air fan revolutions
                               (BG-SET), set value output temperature (AT-SET) */

#define IDENT_SW_ID		0x0d /*!< Software ID */

/* Dataset commands are custom and proprietary to this library */
#define DATASET_COUNT	0x01 /* Amount of data set entries. */
#define DATASET_READ    0x02 /* Read given data set entry. */
#define DATASET_WRITE   0x03 /* Write given data set entry. */

/* 053 operational info indexes */
#define OPINFO_LIMITS 02
/* 
  data format:
 1 byte:  no idea
 2 bytes: Minimum voltage threshold in milivolts
 4 bytes: no idea
 1 byte:  minimum voltage detection delay (seconds)
 2 bytes: maximum voltage threshold in milivolts
 4 bytes: no idea
 1 byte:  maximum voltage detection delay (seconds
 */

/* 0x56 Error code operand 0 */
#define ERR_LIST   1 /* send not data. answer is n, code0, counter0-1, code1, counter1-1 ... coden, countern-1 */
#define ERR_READ   2 /* send code. answer code, flags, counter ... (err_info_t) */
#define ERR_DEL    3 /* send no data. answer also no data. */

/* Error codes */
typedef enum {
  ERR_DEVICE  = 0x01, /* Defective control unit*/
  ERR_NOSTART = 0x02, /* No start */
  ERR_FLAME   = 0x03, /* Flame failure */
  ERR_VCCHIGH = 0x04, /* Supply voltage too high */
  ERR_FLAME2  = 0x05, /* Flame was detected prior to combustion */
  ERR_OH      = 0x06, /* Heating unit overheated  */
  ERR_IL      = 0x07, /* Heating unit interlocked */
  ERR_SCDP    = 0x08, /* Metering pump short circuit */
  ERR_SCCAF   = 0x09, /* Combustion air fan short circuit */
  ERR_SCGP    = 0x0a, /* Glow plug/flame monitor short circuit */
  ERR_SCCP    = 0x0b, /* Circulation pump short circuit */
  ERR_COMAC   = 0x0c, /* No comunication to air condition */
  ERR_SCLEDG  = 0x0d, /* Green LED short circuit */
  ERR_SCLEDY  = 0x0e, /* Yellow LED short circuit */
  ERR_CFG     = 0x0f, /* No configuraton signal */
  ERR_SCSV    = 0x10, /* Solenoid valve short circuit */
  ERR_ECU     = 0x11, /* ECU wrong coded */
  ERR_WBUS    = 0x12, /* W-Bus comunication failure */
  ERR_SCVF    = 0x13, /* Vehicle fan relay short circuit */
  ERR_SCT     = 0x14, /* Temperature sensor short circuit */
  ERR_BLKCAF  = 0x15, /* Combustion air fan blocked */
  ERR_SCBAT   = 0x16, /* Battery main switch short circuit */
  ERR_IAFR    = 0x17, /* Invalid air flow reduction */
  ERR_COMCS   = 0x18, /* Comunication failure on customer specific bus */
  ERR_SCGP2   = 0x19, /* Glow plug/electronic ignition short circuit */
  ERR_SCFD    = 0x1a, /* Flame sensor short circuit */
  ERR_SCOH    = 0x1b, /* Overheat short circuit */
  ERR_28      = 0x1c, /* Fault 28 */
  ERR_SCSV2   = 0x1d, /* Solenoid valve shed test short circuit */
  ERR_SCFS    = 0x1e, /* Fuel sensor short circuit */
  ERR_SCNSH   = 0x1f, /* Nozzle stock heating short circuit */
  ERR_SCOI    = 0x20, /* Operation indicator short circuit */
  ERR_SCFD2   = 0x21, /* Flame indicator short circuit */
  ERR_RREF    = 0x22, /* Reference resistance wrong */
  ERR_CRASH   = 0x23, /* Crash interlock activated */
  ERR_NOFUEL  = 0x24, /* Car is almost out of fuel */
  ERR_SCFPW   = 0x25, /* Fuel pre heating short circuit */
  ERR_SCTPCB  = 0x26, /* PCB temperatur sensor short circuit */
  ERR_ECUGND  = 0x27, /* Ground contact to the ECU broken */
  ERR_LPV     = 0x28, /* Board net energy manager low power voltage */
  ERR_FPSND   = 0x29, /* Fuel priming still not done */
  ERR_TSDATA  = 0x2a, /* Error in the radio telegram */
  ERR_TSSNP   = 0x2b, /* Telestart still not programmed */
  ERR_SCP     = 0x2c, /* The pressure sensor has short circuit */
  ERR_45 = 0x2d, /* Fault 45 */
  ERR_46 = 0x2e, /* ... */
  ERR_47 = 0x2f,
  ERR_48 = 0x30,
  ERR_49 = 0x31, /* Fault 49 */
  ERR_CIDDLE  = 0x32, /* No start from control idle period */
  ERR_FDSIG   = 0x33, /* Flame monitor signal invalid */
  ERR_DEFAULT = 0x34, /* Default values entered */
  ERR_EOLSNP  = 0x35, /* EOL programming has not been carried out */
  ERR_SCFUSE  = 0x36, /* Thermal fuse short circuit */
  ERR_55 = 0x37, /* Fault 55 */
                 /* ... */
  ERR_RELAY   = 0x45, /* Error relay box (short circuit/open circuit of heating relay)*/
  ERR_79 = 0x4f, /* Fault 79 */
  ERR_IDDLEUI = 0x50, /* User interface idle-Mode (no-communication) */
  ERR_COMUI   = 0x51, /* User interface has communication fault */
  ERR_COMUI2  = 0x52, /* User interface send no defined operating mode */
  ERR_FAN     = 0x53, /* Heater fan status message negative */
  ERR_SCFAN   = 0x54, /* Heater fan status bus has short circuit to UB */
  ERR_TW      = 0x55, /* Temperature water sensor failure */
  ERR_SCTW    = 0x56, /* Temperature water sensor short circuit to UB */
  ERR_OHTW    = 0x57, /* Overheating water temperature sensor */
  ERR_OSHT    = 0x58, /* Overstepping water temperature sensor gradient */
  ERR_TBLOW   = 0x59, /* Overheating blow temperature sensor */
  ERR_OSLT    = 0x5a, /* Overstepping low temperature sensor gradient */
  ERR_OHPCBT  = 0x5b, /* Overheating printed circuit board temperature sensor */
  ERR_OSPCBT  = 0x5c, /* Overstepping printed circuit board temp sensor gradient */
  ERR_TC      = 0x5d, /* Cabin temperature sensor failure */
  ERR_OSFD    = 0x5e, /* Flame detector gradient failure */
  ERR_ECOOL   = 0x5f, /* Emergency cooling */
  ERR_CS1  = 0x60, /* Customer specific fault 1 */
  ERR_CS32 = 0x7f, /* Customer specific fault 32 */
  ERR_128 = 0x80, /* Fault 128 */
  ERR_EOLCHK  = 0x81, /* EOL checksum error */
  ERR_TEST    = 0x82, /* No start during test-run */
  ERR_FLAME3  = 0x83, /* Flame failure */
  ERR_VCCLOW  = 0x84, /* Operating voltage too low */
  ERR_FDAFTER = 0x85, /* Flame was detected after combustion */
  ERR_134 = 0x86, /* Fault 134 */
  ERR_PLOCK = 0x87, /* Heater lock-out permanent */
  ERR_DP     = 0x88, /* Fuel pump failure */
  ERR_INTCAF = 0x89, /* Combustion air fan interruption */
  ERR_INTGP  = 0x8a, /* Glow plug / flame monitor interruption */
  ERR_INTCP  = 0x8b, /* Circulation pump interruption */
  ERR_140 = 0x8c, /* Fault 140 */
  ERR_INTLEDG = 0x8d, /* Green LED interruption */
  ERR_INTLEDY = 0x8e, /* Yellow LED interruption */
  ERR_143 = 0x8f, /* Fault 143 */
  ERR_INTSV   = 0x90, /* Solenoid valve interruption */
  ERR_NEUTRAL = 0x91, /* Control unit locked or coded as neutral */
  ERR_REFRESH = 0x92, /* Command refresh failure */
  ERR_147 = 0x93, /* Fault 147 */
  ERR_INTT    = 0x94, /* Temperature sensor interruption */
  ERR_TCAF    = 0x95, /* Combustion air fan tight */
  ERR_150 = 0x96, /* Fault 150 */
  ERR_OHPOS = 0x97, /* Overheat sensor position wrong */
  ERR_152 = 0x98, /* Fault 152 (Power supply interruption) */
  ERR_INTGP2  = 0x99, /* Glow plug / electronic ignition unit interruption */
  ERR_INTFD   = 0x9a, /* Flame sensor interruption */
  ERR_TXSP    = 0x9b, /* Setpoint transmitter invalid */
  ERR_IVLOW   = 0x9c, /* Intelligent undervoltage detection */
  ERR_INTSV2  = 0x9d, /* Solenoid valve shed test interruption */
  ERR_INTFS   = 0x9e, /* Fuel sensor interruption */
  ERR_INTNSH  = 0x9f, /* Nozzle stock heating interruption */
  ERR_INTOI   = 0xa0, /* Operating indicator interruption */
  ERR_INTFD2  = 0xa1, /* Flame indicator interruption */
  ERR_162 = 0xa2, /* Fault 162 */
  ERR_163 = 0xa3, /* Fault 163 */
  ERR_164 = 0xa4, /* Fault 164 */
  ERR_INTFPW  = 0xa5, /* Fuel pre heating interruption */
  ERR_INTPCBT = 0xa6, /* PCB temperature sensor interruption */
  ERR_167 = 0xa7, /* Fault 167 */
  ERR_EMGR    = 0xa8, /* Communication board net energy manager error */
  ERR_169 = 0xa9, /* Fault 169 */
  ERR_WBUSTX  = 0xaa, /* Send on W-Bus not succeed */
  ERR_INTOHS  = 0xab, /* Overheat sensor interruption */
  ERR_P       = 0xac, /* The pressure sensor failure */
  ERR_173 = 0xad, /* Fault 173 */

  ERR_181 = 0xb5, /* Fault 181 */
  ERR_INTFUSE = 0xb6, /* Thermal fuse interrupted */
  ERR_183 = 0xb7, /* Fault 183 */
  ERR_208 = 0xd0, /* Fault 208 */

  ERR_CS33 = 0xe0, /* Customer specific fault 33 */
  ERR_CS63 = 0xfe, /* Customer specific fault 63 */

  ERR_UNKNOWN = 0xff, /* Unknown error code */
} wbus_error_t;

#define CO2CAL_READ	1 /* 3 data bytes: current value, min value, max value. */
#define CO2CAL_WRITE	3 /* 1 data byte: new current value. */


/* Component test device definitions */
#define WBUS_TEST_CF 1 /*!< Combustion Fan */
#define WBUS_TEST_FP 2 /*!< Fuel Pump */
#define WBUS_TEST_GP 3 /*!< Glow Plug */
#define WBUS_TEST_CP 4 /*!< Circulation Pump */
#define WBUS_TEST_VF 5 /*!< Vehicle Fan Relays */
#define WBUS_TEST_SV 9 /*!< Solenoid Valve */
#define WBUS_TEST_NSH 13 /*!< Nozzel Stock Heating */
#define WBUS_TEXT_NAC 14 /*!< Nozzle air compressor (not standart, POELI specific) */
#define WBUS_TEST_FPW 15 /*!< Fuel Prewarming */

/* Data format / unit translation macros */
static inline void SWAP(unsigned char *out, unsigned short in)
{
  out[1] = (in &  0xff);
  out[0] = (in >> 8) & 0xff;
}

static inline void WORDSWAP(unsigned char *out, unsigned char *in)
{
  /* No pointer tricks, to avoid alignment problems */
  out[1] = in[0]; 
  out[0] = in[1];
}

static inline short twobyte2word(unsigned char *in)
{
  short result;
  
  WORDSWAP((unsigned char*)&result, in);
  return result;
}

/*
 * Convert a short representing mili something (volt, ohm)
 * to a decimal string featuring a nice dot :)
 *
 * Why? Because many uC sprintf implementations do not support
 * floats.
 */
static inline int shortToMili(char *t, short x)
{
  sprintf(t, "%06d", x);
  t[0] = t[1];
  t[1] = t[2];
  t[2] = '.';
  return 6;
}

static inline int byteToMili(char *t, unsigned char x)
{
  sprintf(t, "%05d", x);
  t[0] = t[1];
  t[1] = '.';
  return 5;   
}


#define PERCENT2WORD(w, p) SWAP(&w, (short)(p)*2)
#define FREQ2WORD(w, f)    SWAP(&w, (short)((f)*20) )

#define TEMP2BYTE(x)	  ((x)+50)
#define BYTE2TEMP(x)	  ((unsigned char)((x)-50))

#define VOLT2WORD(w,v)	  SWAP(&w, v)
#define WORD2VOLT_TEXT(t, w) shortToMili(t, twobyte2word(w))

#define WORD2FPWR_TEXT(t, w) shortToMili(t, twobyte2word(w))

#define HOUR2WORD(w, h)	  SWAP(&w, h)
#define WORD2HOUR(w)	  twobyte2word(w)

#define BYTE2GPR_TEXT(t, b)  byteToMili(t, b)

#define WATT2WORD(w, W)    SWAP(&w, W)
#define WORD2WATT(w)       twobyte2word(w)
#define OHM2WORD(w, o)     SWAP(&w, o)

#define RPM2WORD(w, r)     SWAP(&w, r)
#define WORD2RPM(w)        twobyte2word(w)
