######################################################################
# Get Linux Build Enviornment:
include ../Rules.make

DESTDIR		?= ../out
LIB_INSTALL	:= $(DESTDIR)/lib

######################################################################
# Build options
CFLAGS		+= -fpic
INCLUDE		+= -I../include -I../kernel-headers

######################################################################
# Target
COBJS		:= ion.o
CPPOBJS		:=

ION_OBJS	:= ion.o
ION_LIBNAME	:= libion
ION_TARGET	:= $(ION_LIBNAME).a

######################################################################
# Build
OBJS		:= $(COBJS) $(CPPOBJS)

all: $(ION_TARGET)

$(ION_TARGET): $(ION_OBJS)
	$(AR) $(ARFLAGS) $(ION_LIBNAME).a $<

clean:
	rm -f *.o *.a *.so .depend

install:
	install -m 755 -d $(LIB_INSTALL)
	install -m 644 $(ION_TARGET) $(LIB_INSTALL)

distclean: clean
	rm -f $(LIB_INSTALL)/$(ION_LIBNAME).a

######################################################################
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

