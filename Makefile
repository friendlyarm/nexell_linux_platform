# -----------------------------------------------------------------------
# Makefile for building nexell libs and camera demo
#
# Copyright 2016 FriendlyARM (http://www.friendlyarm.com/)
#

# Do not:
# o  use make's built-in rules and variables
#    (this increases performance and avoids hard-to-debug behaviour);
# o  print "Entering directory ...";
MAKEFLAGS +=

# -----------------------------------------------------------------------
# Checking host machine

ifneq (armv7l,$(shell uname -m))
CROSS_COMPILE ?= arm-linux-
GCC           := $(CROSS_COMPILE)gcc

# Checking gcc version, it *MUST BE* 4.9.3 for cross compile
GCC_VERSION_A = $(shell $(GCC) -dumpversion)
ifneq (4.9.3,$(GCC_VERSION_A))
$(warning *** $(GCC) $(GCC_VERSION_A) is *NOT* supported, please)
$(warning *** switch it to "4.9.3" and try again.)
# $(error stopping build)
endif

else
CROSS_COMPILE ?=
endif # End of machine - `armv7l'

export CROSS_COMPILE

# -----------------------------------------------------------------------

LIBS := libion libnxmalloc libnxv4l2 libnxvpu
APPS := nanocams

SUBDIRS  = $(LIBS) $(APPS)

PHONY   += $(SUBDIRS)

# -----------------------------------------------------------------------
PHONY   += all
all: $(SUBDIRS)


$(SUBDIRS):
	@if [ -f $@/Makefile ]; then \
	  $(MAKE) -C $@; \
	fi

libnxmalloc: libion

libnxvpu: libnxv4l2

nanocams: $(LIBS)


install: $(SUBDIRS)
	@for d in $(SUBDIRS); do $(MAKE) -C $${d} $@; done

clean distclean:
	@for d in $(SUBDIRS); do $(MAKE) -C $${d} $@; done

# -----------------------------------------------------------------------

# Declare the contents of the .PHONY variable as phony.  We keep that
# information in a variable so we can use it in if_changed and friends.
.PHONY: $(PHONY) install clean distclean

# End of file
# vim: syntax=make

