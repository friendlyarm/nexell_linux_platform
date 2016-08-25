########################################################################
# Get Linux Build Enviornment:
include ../Rules.make

DESTDIR     ?= ../out
LIB_INSTALL := $(DESTDIR)/lib

######################################################################
# Build options
INCLUDE += -I../libion -I../include -I../kernel-headers

LIBRARY += -L../libion
LIBRARY += -lion

######################################################################
# Target
COBJS   :=
COBJS   += nx_alloc_mem_ion.o
CPPOBJS :=

CFLAGS  += -Wall -O2 -g -fpic

LIBNAME := libnxvmem
TARGET  := libnxvmem.so

######################################################################
# Build
OBJS    := $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $(LIBNAME).a $^
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^ $(LIBRARY)

install:
	install -m 755 -d $(LIB_INSTALL)
	install -m 644 $(LIBNAME).a $(LIB_INSTALL)
	install -m 755 $(TARGET) $(LIB_INSTALL)

clean:
	rm -f *.o *.so *.a .depend

distclean: clean
	rm -f $(LIB_INSTALL)/$(LIBNAME).a
	rm -f $(LIB_INSTALL)/$(TARGET)


#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
else
$(OBJS): depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -MM $(CFLAGS) $(INCS) $(SRCS) > .depend

