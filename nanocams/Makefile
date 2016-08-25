# -----------------------------------------------------------------------
# Makefile for building Camera Demo
#
# Copyright 2016 FriendlyARM (http://www.friendlyarm.net/)
#

ifndef DESTDIR
DESTDIR			?= ../out
endif

CROSS_COMPILE	?= arm-linux-
CC				 = $(CROSS_COMPILE)gcc
CPP				 = $(CROSS_COMPILE)g++
INSTALL			 = install


# -----------------------------------------------------------------------

CFLAGS			+= -Wall -O2
CFLAGS			+= -I../include -I../kernel-headers

ifeq ($(USE_STATIC_LIB),y)
LIBS			+= \
					../libnxvpu/libnxvpu.a \
					../libnxv4l2/libv4l2-nexell.a \
					../libnxmalloc/libnxvmem.a \
					../libion/libion.a
else
LIBS			+= -L../libion -lion
LIBS			+= -L../libnxmalloc -lnxvmem
LIBS			+= -L../libnxv4l2 -lv4l2-nexell
LIBS			+= -L../libnxvpu -lnxvpu
endif
LIBS			+= -L../prebuilt -lnxvidrc


# -----------------------------------------------------------------------

TARGET			 = nanocams


all: $(TARGET)

%.o: %.cpp
	$(CPP) $(CFLAGS) -c -o $@ $<

nanocams: NXJpegHWEnc.o nanocams.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS) -lstdc++


install: $(TARGET)
	$(INSTALL) -d 755 $(DESTDIR)/bin
	$(INSTALL) $^ $(DESTDIR)/bin/

clean distclean:
	rm -rf *.o $(TARGET)


# -----------------------------------------------------------------------

.PHONY: $(PHONY) install clean distclean

# End of file
# vim: syntax=make

