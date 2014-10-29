# Build libwbus and commandline tool.

VPATH = %.c ./wbus ./kernel ./openegg ./util ./poeli ./ph
.PHONY: libwbus clean

# Assume some default hardware configuration options. Only relevant for poeli.
ifeq "$(PSENSOR)" ""
# 1: MPX5100, 2: MPX4250
 PSENSOR=1
 $(warning Assuming PSENSOR=$(PSENSOR))
endif
ifeq "$(NOZZLE)" ""
# 1: Hago SN609-02, 2: Delavan SN30609-01
 NOZZLE=1
 $(warning Assuming NOZZLE=$(NOZZLE))
endif
ifeq "$(CAF)" ""
# 1: Toy, 2: DBW46
 CAF=1
 $(warning Assuming CAF=$(CAF))
endif

CFLAGS = -DPSENSOR=$(PSENSOR) -DCAF_TYPE=$(CAF) -DNOZZLE=$(NOZZLE)

# Differentiate between egg or poeli type hardware.
ifeq "$(VARIANT)" ""
	VARIANT=poeli # egg
	$(warning Assuming VARIANT=$(VARIANT))
endif

ifeq "$(VARIANT)" "egg"
	CFLAGS += -DEGG_HW
	PROGRAMS = $(BINDIR)/openegg$(EXE_SUFFIX)
else ifeq "$(VARIANT)" "poeli"
	CFLAGS += -DPOELI_HW
	PROGRAMS = $(BINDIR)/poeli$(EXE_SUFFIX)
else ifeq "$(VARIANT)" "ph"
	CFLAGS += -DPH_HW
	PROGRAMS = $(BINDIR)/ph$(EXE_SUFFIX)
endif

#CFLAGS += -g -O2 -finline-functions -Wall -Iinclude -Isrc
ifeq "$(DEBUG)" ""
CFLAGS += -O2 -Wall -Iinclude -Isrc
LDFLAGS += 
else
CFLAGS += -g -O0 -Wall -Iinclude -Isrc   
LDFLAGS += -g
endif

# Default to posix machine
ifeq "$(ARCH)" ""
CC=gcc
LD=ld
CFLAGS += -I./include
CFLAGS_htsim_gui = $(shell pkg-config --cflags libglade-2.0)
LDFLAGS_htsim_gui = $(shell pkg-config --libs libglade-2.0) -Wl,--export-dynamic
CFLAGS_seq_edit = $(shell pkg-config --cflags libglade-2.0)
LDFLAGS_seq_edit = $(shell pkg-config --libs libglade-2.0) -Wl,--export-dynamic
CFLAGS_htsim = $(shell pkg-config --cflags glib-2.0)
LDFLAGS_htsim = $(shell pkg-config --libs glib-2.0)
LDFLAGS += -lpthread -lc
PROGRAMS += $(BINDIR)/wbtool$(EXE_SUFFIX) $(BINDIR)/wbsim$(EXE_SUFFIX) $(BINDIR)/htsim$(EXE_SUFFIX) util/htsim_gui$(EXE_SUFFIX) util/seq_edit$(EXE_SUFFIX)
EXE_SUFFIX=
endif

# MSP430 uC
ifeq "$(findstring msp430,$(ARCH))" "msp430"
CC=msp430-gcc
LD=msp430-ld
CFLAGS += -g -mmcu=$(ARCH) -I./include -ffunction-sections -fdata-sections
LDFLAGS = -g -mmcu=$(ARCH) -Wl,--section-start -Wl,.flashrw=0xf400 -Wl,--gc-sections -lc
AR=msp430-ar
RANLIB=msp430-ranlib
EXE_SUFFIX=
endif

# MingW (Windows)
ifneq "$(findstring mingw,$(ARCH))" ""
CC=$(ARCH)-gcc
LD=$(ARCH)-ld
CFLAGS += -g -I./include
LDFLAGS = -g -lwinmm -lws2_32
AR=$(ARCH)-ar
RANLIB=$(ARCH)-ranlib
EXE_SUFFIX=.exe
PROGRAMS += $(BINDIR)/wbtool$(EXE_SUFFIX) $(BINDIR)/wbsim$(EXE_SUFFIX)
endif

# ARM
ifneq "$(findstring arm,$(ARCH))" ""
CC=$(ARCH)-gcc
LD=$(ARCH)-ld
CFLAGS += -g -I./include
LDFLAGS = -g
AR=$(ARCH)-ar
RANLIB=$(ARCH)-ranlib
EXE_SUFFIX=
PROGRAMS += $(BINDIR)/wbsim$(EXE_SUFFIX)
endif

# Other uC
ifeq "$(findstring avr,$(ARCH))" "avr"
$(error not yet implemented. Come on, do it ! Its not that hard !)

endif

OBJDIR = ./obj$(ARCH)$(VARIANT)
BINDIR = ./bin$(ARCH)$(VARIANT)
LIBDIR = ./lib$(ARCH)$(VARIANT)


all: dirs $(PROGRAMS)

$(LIBDIR)/libopenegg.a: $(OBJDIR)/openegg_ui.o $(OBJDIR)/openegg_menu.o $(OBJDIR)/gsmctl.o
	$(AR) cru $@ $^

$(LIBDIR)/libkernel.a: $(OBJDIR)/kernel.o $(OBJDIR)/rs232.o $(OBJDIR)/machine.o
	$(AR) cru $@ $^

# Dependencies
$(OBJDIR)/openegg_ui.o: ./openegg/openegg_ui_posix.c ./openegg/openegg_ui_msp430.c ./openegg/openegg_ui_win32.c ./openegg/openegg_ui.h ./include/kernel.h ./include/wbus_server.h
$(OBJDIR)/openegg.o: ./include/machine.h ./include/kernel.h
$(OBJDIR)/rs232.o: ./kernel/rs232_posix.c ./kernel/rs232_msp430.c ./kernel/rs232_win32.c ./include/rs232.h ./include/kernel.h
$(OBJDIR)/machine.o: ./kernel/machine_posix.c ./kernel/machine_msp430.c ./kernel/machine_win32.c ./include/machine.h ./include/kernel.h
$(OBJDIR)/poeli_ctrl.o: ./poeli/poeli_ctrl_msp430.c ./poeli/poeli_ctrl_posix.c ./include/poeli_ctrl.h ./include/machine.h ./include/kernel.h
$(OBJDIR)/wbus.o: ./include/rs232.h ./include/wbus.h ./wbus/wbus_const.h ./include/kernel.h
$(OBJDIR)/wbus_server.o: ./include/rs232.h ./include/wbus.h ./wbus/wbus_const.h ./include/kernel.h
$(OBJDIR)/iso.o: ./include/iso.h ./include/kernel.h ./include/rs232.h
$(OBJDIR)/poeli.o: ./include/wbus_server.h ./include/poeli_ctrl.h ./include/machine.h

$(OBJDIR)/htsim.o: htsim.c
	$(CC) -c $(CFLAGS) $(CFLAGS_htsim) -o $@ $<

$(OBJDIR)/htsim_gui.o: htsim_gui.c
	$(CC) -c $(CFLAGS) $(CFLAGS_htsim_gui) -o $@ $<

$(OBJDIR)/seq_edit.o: seq_edit.c
	$(CC) -c $(CFLAGS) $(CFLAGS_seq_edit) -o $@ $<

$(OBJDIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_$(%)) -o $@ $<

$(BINDIR)/openegg$(EXE_SUFFIX): $(OBJDIR)/openegg.o $(OBJDIR)/wbus.o $(LIBDIR)/libopenegg.a $(LIBDIR)/libkernel.a
	$(CC) -o $@ $^ $(LDFLAGS)

$(BINDIR)/htsim$(EXE_SUFFIX): $(OBJDIR)/htsim.o $(OBJDIR)/wbus.o $(OBJDIR)/wbus_server.o $(LIBDIR)/libkernel.a
	$(CC) $(LDFLAGS_htsim) -o $@ $^ $(LDFLAGS)

$(BINDIR)/poeli$(EXE_SUFFIX): $(OBJDIR)/poeli.o $(OBJDIR)/wbus.o $(OBJDIR)/wbus_server.o $(OBJDIR)/poeli_ctrl.o $(LIBDIR)/libkernel.a
	$(CC) -o $@ $^ $(LDFLAGS)

$(BINDIR)/wbtool$(EXE_SUFFIX): $(OBJDIR)/wbtool.o $(OBJDIR)/wbus.o $(LIBDIR)/libkernel.a
	$(CC) -o $@ $^ $(LDFLAGS)

$(BINDIR)/wbsim$(EXE_SUFFIX): $(OBJDIR)/wbsim.o $(OBJDIR)/wbus.o $(LIBDIR)/libkernel.a
	$(CC) -o $@ $^ $(LDFLAGS)

util/htsim_gui$(EXE_SUFFIX): $(OBJDIR)/htsim_gui.o
	$(CC) $(LDFLAGS_htsim_gui) -o $@ $^ $(LDFLAGS)

util/seq_edit$(EXE_SUFFIX): $(OBJDIR)/seq_edit.o $(OBJDIR)/wbus.o $(LIBDIR)/libkernel.a
	$(CC) $(LDFLAGS_seq_edit) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(PROGRAMS) $(LIBS) $(OBJDIR)/*.o

dirs: ;
	mkdir -p $(BINDIR) $(LIBDIR) $(OBJDIR)
