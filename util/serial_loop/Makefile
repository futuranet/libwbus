VER_MAJOR := $(shell uname -r |cut -d "." -f 1)
VER_MINOR := $(shell uname -r |cut -d "." -f 2)
KERNELNEW := $(shell if [ $(VER_MAJOR)$(VER_MINOR) -gt 25 ]; then echo 1; else echo 0; fi)
NEW:=1

all:: serial_loop_test

ifeq ($(KERNELNEW),$(NEW))

obj-m	:= serial_loop.o

INCLUDE	:= -I/usr/include/asm/mach-default/
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)

all::
	$(MAKE) -C $(KDIR) $(INCLUDE) SUBDIRS=$(PWD) modules
	
else

TARGET	:= serial_loop
INCLUDE	:= -I/lib/modules/`uname -r`/build/include -I/usr/include/asm/mach-default/
CFLAGS	:= -O2 -Wall -DMODULE -D__KERNEL__ -DLINUX
CC	:= gcc

all:: ${TARGET}.o

${TARGET}.o: ${TARGET}.c
	$(CC) $(CFLAGS) ${INCLUDE} -c ${TARGET}.c

	
endif

clean:
	rm -f serial_loop.o serial_loop.ko serial_loop.cmd.c

serial_loop_test: serial_loop_test.c
	gcc -o $@ $<
