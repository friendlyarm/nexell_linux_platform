# -----------------------------------------------------------------------
# Makefile for building Camera Demo
#
# Copyright 2016 FriendlyARM (http://www.friendlyarm.net/)
#

OPENCVLIB=-lopencv_core -lopencv_highgui -lopencv_imgproc

ifndef DESTDIR
DESTDIR			?= ../out
endif

ifneq (armv7l,$(shell uname -m))
CROSS_COMPILE	?= arm-linux-
else
CROSS_COMPILE	?=
endif
CC				 = $(CROSS_COMPILE)gcc
CPP				 = $(CROSS_COMPILE)g++
INSTALL			 = install


# -----------------------------------------------------------------------

CFLAGS			+= -Wall -O2 -std=c++11
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
LIBS			+= -L../prebuilt -lnxvidrc $(OPENCVLIB)


# -----------------------------------------------------------------------

TARGET			 = nanocam_opencv


all: $(TARGET)

%.o: %.cpp
	$(CPP) $(CFLAGS) -c -o $@ $<

nanocam_opencv: nanocam_opencv.o
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

