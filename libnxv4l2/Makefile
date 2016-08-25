######################################################################
# Get Linux Build Enviornment:
include ../Rules.make

DESTDIR		?= ../out
LIB_INSTALL	:= $(DESTDIR)/lib

######################################################################
# Build Options
CFLAGS		:= -Wall -O2 -g -fpic
CPPFLAGS	:= -Wall -O2 -g -fpic
INCLUDE		+= -I. -I../include -I../kernel-headers
LIBRARY		+= -L. -lnxv4l2

######################################################################
# Target
# nxp-v4l2.cpp 			: v4l2 original libaray
# nxp-v4l2-media.cpp	: v4l2 libaray for media-solution team
COBJS			:=
CPPOBJS			:= nxp-v4l2.o nxp-v4l2-media.o nxp-v4l2-dev.o nx_vip.o nx_dsp.o

NXV4L2_OBJS		:= nxp-v4l2.o nxp-v4l2-dev.o
NXV4L2_LIBNAME	:= libv4l2-nexell
NXV4L2_TARGET	:= $(NXV4L2_LIBNAME).so

V4L2_OBJS		:= nxp-v4l2-media.o nxp-v4l2-dev.o
V4L2_LIBNAME	:= libnxv4l2
V4L2_TARGET		:= $(V4L2_LIBNAME).so

VIP_OBJS		:= nx_vip.o
VIP_LIBNAME		:= libnxvip
VIP_TARGET		:= $(VIP_LIBNAME).so

DSP_OBJS		:= nx_dsp.o
DSP_LIBNAME		:= libnxdsp
DSP_TARGET		:= $(DSP_LIBNAME).so

######################################################################
# Build
OBJS			:= $(COBJS) $(CPPOBJS)

all: $(NXV4L2_TARGET) $(V4L2_TARGET) $(VIP_TARGET) $(DSP_TARGET)

$(NXV4L2_TARGET): $(NXV4L2_OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^
	$(AR) $(ARFLAGS) $(NXV4L2_LIBNAME).a $^

$(V4L2_TARGET): $(V4L2_OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^
	$(AR) $(ARFLAGS) $(V4L2_LIBNAME).a $^

$(VIP_TARGET): $(VIP_OBJS) $(V4L2_TARGET)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $(VIP_OBJS) $(LIBRARY)
	$(AR) $(ARFLAGS) $(VIP_LIBNAME).a $(VIP_OBJS)

$(DSP_TARGET): $(DSP_OBJS) $(V4L2_TARGET)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $(DSP_OBJS) $(LIBRARY)
	$(AR) $(ARFLAGS) $(DSP_LIBNAME).a $(DSP_OBJS)

clean:
	rm -f *.o *.a *.so *.so.* .depend

install:
	install -m 755 -d $(LIB_INSTALL)
	install -m 755 $(NXV4L2_TARGET) $(LIB_INSTALL)
	install -m 755 $(V4L2_TARGET) $(LIB_INSTALL)
	install -m 755 $(VIP_TARGET) $(LIB_INSTALL)
	install -m 755 $(DSP_TARGET) $(LIB_INSTALL)

distclean: clean
	rm -f $(LIB_INSTALL)/$(NXV4L2_TARGET)
	rm -f $(LIB_INSTALL)/$(V4L2_TARGET)
	rm -f $(LIB_INSTALL)/$(VIP_TARGET)
	rm -f $(LIB_INSTALL)/$(DSP_TARGET)

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
else
$(OBJS): depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC) -MM $(CFLAGS) $(INCS) $(SRCS) > .depend

