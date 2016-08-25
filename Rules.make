#########################################################################
# Embedded Linux Build Enviornment:
#
OBJTREE		:= $(if $(BUILD_DIR),$(BUILD_DIR),$(CURDIR))
BASEDIR		:= $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

#########################################################################
# Toolchain
CROSS_COMPILE	?= arm-linux-
CC			:= $(CROSS_COMPILE)gcc
CXX			:= $(CROSS_COMPILE)g++
AR			:= $(CROSS_COMPILE)ar
AS			:= $(CROSS_COMPILE)as
LD			:= $(CROSS_COMPILE)ld
NM			:= $(CROSS_COMPILE)nm
RANLIB		:= $(CROSS_COMPILE)ranlib
OBJCOPY		:= $(CROSS_COMPILE)objcopy
STRIP		:= $(CROSS_COMPILE)strip

#########################################################################
# Library & Header macro
INCLUDE		:=

#########################################################################
# Build Options
OPTS		:= -O2 -Wall -Wextra \
			   -Wcast-align -Wno-unused-parameter -Wshadow -Wwrite-strings \
			   -Wcast-qual -fno-strict-aliasing -fstrict-overflow -fsigned-char \
			   -fno-omit-frame-pointer -fno-optimize-sibling-calls
COPTS		:= $(OPTS)
CPPOPTS		:= $(OPTS) -Wnon-virtual-dtor

CFLAGS		:= $(COPTS)
CPPFLAGS	:= $(CPPOPTS)
AFLAGS		:=

ARFLAGS		:= crv
LDFLAGS		:=
LIBRARY		:=

#########################################################################
# Generic Rules
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.s
	$(AS) $(AFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDE) -c -o $@ $<

