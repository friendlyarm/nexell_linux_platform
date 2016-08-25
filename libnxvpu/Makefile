########################################################################
# Get Linux Build Enviornment:
include ../Rules.make

DESTDIR		?= ../out
LIB_INSTALL	:= $(DESTDIR)/lib

######################################################################
# Build options
INCLUDE	+= -I./ -I../include -I../kernel-headers

LIBRARY	+= -L../libnxmalloc -lnxvmem
LIBRARY	+= -L../prebuilt -lnxvidrc

ifeq ($(HEVC_DEC),y)
CFLAGS	+= -DHEVC_DEC
INCLUDE	+= -I../libhevc/common -I../libhevc/decoder
LIBRARY	+= -lhevcdec
endif

######################################################################
# Target
COBJS	:= parser_vld.o nx_video_api.o
CPPOBJS	:=

CFLAGS	+= -fPIC

LDFLAGS	+= -Wl,-Bsymbolic

LIBNAME	:= libnxvpu
TARGET	:= libnxvpu.so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $(LIBNAME).a $^
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^ $(LIBRARY)

install:
	install -m 755 -d $(LIB_INSTALL)
	install -m 755 $(TARGET) $(LIB_INSTALL)
	install -m 644 $(LIBNAME).a $(LIB_INSTALL)

clean:
	rm -f *.o *.so *.a .depend

distclean: clean
	rm -f $(LIB_INSTALL)/$(TARGET)
	rm -f $(LIB_INSTALL)/$(LIBNAME).a


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

