== Disclaimer ==

This document contains information that might be wrong, not serve any
purpose or perhaps cause direct or indirect damage to any person. I cannot
be held responsible for anything in regard to this text document, the
accompanied documents or anything else related. Its all your own fault.

Any reference to any trade mark mentioned here or in the accompanied
documents are owned by their corresponding owners.

== What is this ? ==

A collection of W-Bus related libraries and programs for x86 and embedded
microcontroller devices.

== What is W-Bus ? ==

W-Bus is the serial line protocol used by Webasto (tm) car parking and
suplemental heating devices to communicate to a remote control or timer
unit, and also to performe PC assisted diagnosis of the device. This
interface is available on many modern Webasto (tm) heater devices.

== What W-Bus devices are supported to communicate to ? ==

Basic packet communication to any W-Bus capable device should be feasible.
Command codes, addresses or transaction IDs (how name them ?) are only known
so far for Thermo Top V heater devices. Some data has been discovered in
regard to the "1533" timer command devices and in the source forge forum
somebody also found some data in regard to Telestart (tm) receivers. I do
not own such devices so I cannot test anything in regard to that. Software
patches to add support for other devices are gladly accepted as long as the
changes do not add extra resource costs to the existing functionality and do
not include any unnecessary changes. Any patch containing "beautifications"
or any non functional changes will land directly in /dev/null. Don't loose
the point ;)

== What hardware does this software iself run on ? ==

The libwbus library and all its bundled applications should run on x86 Posix
like environment and MSP430 microcontrollers out of the box. Windows support
without cygwin is somewhat supported but there are known missing/broken
things.
Adapting the software for other targets should not be that difficult, just
grab an existing implementation and change it as necessary. For the hardware
and platform specific parts there are source code files using ad hoc
suffixes, and these source code files are included from source code files
without that suffixes. Just take a look and you will find out that quickly.
Doing this way, its not required changing makefiles or anything else to
switch hardware/platform, and thus only compiler built in macros should be
used to detect a given hardware or platform.

== K-Line adapter hardware ==
A K-Line is nothing but a single line serial connection, using a open
colector transistor that pulls a 12 Volt line to ground for mark bits.
At iddle, the line is at 12 Volt. As a pull up, 1Kohm should work ok.
See k_line.png for an example. The nets marked as TX and RX go to the
corresponding microcontroller serial port pins. The schematic shows an
example where the uC has 3.3 Volt I/O. You may need to adapt the voltage
levels depending on what hardware you do use.

== What is that Can-Bus thing ? ==

CAN-BUS is a network protocol for Controller Area Networks (hence the name).
It is widely used in the automotive world for communication between
different controller equipments.
Some people say that some Webasto (tm) devices can't be accessed through
W-Bus but only Can-Bus. That is not true, those devices are programmed at
the factory as suplemental heaters only, thus wont accept a parking heater
command on the W-Bus wire, but all others.
Possibly the reason is to avoid any cheap upgrade to a parking heater being
done by smart customers. Depending on for which car the heater was intended
the parking heater functionality might be enabled or not. From the hardware
perspective suplemental heaters and parking heater are identical AFAIK. The
firmware is different; this can be observed in the "W-Bus code" number.
Some car manufacturers sell upgrade packages which include a single use
dongle to enable parking heater functionality in suplemental heaters.

== Do you have a crack for the upgrade software dongle ? ==

No, I do not.
Its much easier to sell a suplemental heater on some auction house and buy
a parking heater for the same money.

== What is included in the repository ? ==

This software package comprises the following programs:

- openegg: W-Bus heater control unit targeted to a slightly modified MTK449
  olimex kit board. Schematics available on request, but I'm planning to
  rework the hardware at some point, but also using a MSP430 uC.
    The device when completely assembled provides through a menu driven UI the
  possibility to set 3 timers to turn on a W-Bus heater device. It also
  allows to select the command to be used, either parking heating,
  suplemental heating or ventilation.
  The heater can be turned on explicitly as well.
    Also it includes the function to connect a GSM mobile phone supporting
  the caller-ID AT commands, to program upto to 4 phone numbers to be
  authorized to turn on a W-Bus heater by calling the connected GSM mobile
  phone. The phone wont pick up the call, merely read out the caller ID number,
  thus no phone cost should ocurr.

- util/wbtool: a command line tool intended to be used on a PC or similar posix
  compatible system, to interact with a W-Bus device. Currently it supports
  all libwbus commands, thus turn on/off heater, read sensors, device ID
  data, etc.

- util/wbsim: a rudimentary and simple W-Bus device simulator mostly useful for
  understanding W-Bus and testing. In the W-Bus sense, its the counter part
  of wbtool.

- poeli: This program is intended to be used on an embedded heater device
  controller. It is designed for syphon nozzle atomizing burners with a
  nozzle stock heater, glow plug for ignition and flame monitoring,
  combustion air fan, nozzle air pressure compressor, 2 temperatur sensors,
  heat exchanger circulation pump, pulsed fuel pump (as usual in parking
  heaters), vehicle cabin fan output, and an auxiliar output. It acts as a
  W-Bus heater device, thus can be controlled and monitored using the
  Webasto (tm) Data Top (tm) software.
  A sequencer which is programmable via W-Bus protocol as well defines the
  course of action once a "heater on" or ventilation command is issued. Also
  the regular W-Bus component test commands and CO2 calibration are
  supported. The "program" can also be built and run on a PC for simulation
  purposes. See htsim.
  Circuit schematics are not included in the repository, and I'm not sure if
  I should really publish them. If you have any interest building a heater
  device, post it in the source forge forum, maybe there is some way to help
  you in that case. Heater device do burn fuel producing some kilo watt of
  heat and thus are DANGEROUS. Unless you plan to do something really stupid
  you are better off buying a ready made heater, building one is by far more
  expensive (easily a factor 4 of the price of an off the shelf device).

- bin/htsim: Heater device sensor / actuator simulator. A very simple basic
  and inaccurate model of a heater device. It uses shared memory to communicate
  to an instance of "poeli". It takes actuator values and generates data for
  the sensors. For example if the fuel pump is turned on, the simulator
  assumes combustion and will slowly rise the water temperatur sensor value
  and flame detector.

- util/htsim_gui: Same as htsim but interactive. Allows changing sensor
  values and watching actuator values via shared memory from an instance of
  "poeli". Since clicking on scrollbars takes a lot of time, rather less
  usefull. I never used it really, htsim turned out to be much more
  practical. Uses GTK+ and glade. Must be run from the "util" directory in
  order that the program can find its GUI description XML file.

- util/seq_edit: This programm allows to change almost all parameters of the
  poeli sequencer and read and write them via W-Bus to a heater device. This
  is not compatible with original Webasto (tm) heaters, since I do not know if
  and how they can be reprogrammed in this sense.

Libraries and others:

- libwbus: the wbus protocol handling library. see include/wbus.h for the
  client side API and include/wbus_server.h for the server side API.

- kernel: small cooperative multitasking micro kernel, to enable small
  microcontrollers to execute several tasks and blocking i/o. It also
  manages hardware abstraction in order to minimize the effort to port the
  whole software package to other processor architectures. Only 2-3 hardware
  specific lines of code.

- util/serial_loop: Linux kernel module which simulates 8 serial ports,
  which are looped back pairwise. It supports optional local echo, as it
  is the case in a K-Line or W-Bus line. It creates 8 device nodes called
  /dev/ttySL0 upto /dev/ttySL7, where /dev/ttySL0 is linked with
  /dev/ttySL1, and /dev/ttySL2 with /dev/ttySL3 and so on.
  The module should built with any recent 2.6 Linux kernel.

- Matlab files and excel spreadsheets: design blueprints and simulations
  that were used during the design and tuning of different parts of the
  heater device project. The Matlab files can be run directly as programs
  from a shell of GNU/Octave is installed.

== How do I compile this shit ? ==

Native compile:

make

Cross compile for MSP430f169:

make ARCH=msp430x169

To compile with debug symbols add DEBUG=1 to the command line.

the executables will be placed in bin<architecture name>, thus in
"binmsp430x169" for the example above. In case of native built in the folder
"bin".

The seq-edit and htsim_gui executables will be placed in util/ by the make
file, since it requires to load an XML file which is located there. Those
programs should be executed using util/ as working directory as well.

== How do I port this to my favorit uC since I hate that shitty MSP430 ? ==

- Re implement the functions and macros declared in the files rs232.h and
machine.h for your uC. Take the "posix" built variant as a template.
- Then put the implemention of that functions in new files called
rs232_myuc.c and machine_myuc.c. Where "myuc" somehow identifies your new uC
architechture or built environment. For example rs232_avr.c ...
- Make sure that those files are inluded in rs232.c and machine.c in case
your compiler for your uC is being used (consult predefined macros of your
compiler)
- Change the the Makefile CFLAGS and LDFLAGS if required.
- If you do not break functionality nor compilability for all other
architectures I might include the changes into CVS if you want that. If you
change anything out of scope, I wont do that.

If you are completely lost, don't start whining but improve your skills. You
might have to be pacient for that occurring of course, and have fun doing that :)


W-Bus version: 3.3
Device Name: PQ35 ZH 
W-Bus code: 213cc0e73f0000
Device ID Number: 0900620743
Data set ID Number: 090074764500
Software ID Number: 0000000030
Hardware version: 38/2
Software version: Monday 16/3  4.7
Software version EEPROM: Monday 16/3  4.7
Date of Manufacture Control Unit: 14.9.3
Date of Manufacture Heater: 1.10.3
Customer ID Number: 1K0815071E 0707
Serial Number: 0000349144
Sensor8: 42f0d004081858
U1: 0b0000000003bf
System Level: 58

== How was that damn command line to program my heater device ? ==

LIBMSPGCC_PATH=/usr/local/lib /home/mjander/source/msp430/python/msp430-jtag.py -e binmsp430x149/poeli
