# Copyright 2014-2016, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# src/tools/Makefile.inc -- Makefile include for all tools
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..

include $(TOP)/src/common.inc

INSTALL_TARGET ?= y

INCS += -I.
INCS += -I$(TOP)/src/include
CFLAGS += -std=gnu99
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith
CFLAGS += -Wunused-macros
CFLAGS += -Wmissing-field-initializers
CFLAGS += -Wsign-conversion
CFLAGS += -Wsign-compare
ifeq ($(call check_Wconversion), y)
CFLAGS += -Wconversion
endif
CFLAGS += -fno-common

CFLAGS += -DSRCVERSION='"$(SRCVERSION)"'
ifeq ($(call check_flag, -Wunreachable-code-return), y)
CFLAGS += -Wunreachable-code-return
endif
ifeq ($(call check_flag, -Wmissing-variable-declarations), y)
CFLAGS += -Wmissing-variable-declarations
endif

ifeq ($(DEBUG),1)
CFLAGS += -ggdb $(EXTRA_CFLAGS_DEBUG)
else
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 $(EXTRA_CFLAGS_RELEASE)
endif

CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS += -Wl,-z,relro -Wl,--warn-common -Wl,--fatal-warnings $(EXTRA_LDFLAGS) -L$(TOP)/src/nondebug
TARGET_DIR=$(DESTDIR)$(bindir)
BASH_COMP_FILES ?=
BASH_COMP_DESTDIR = $(DESTDIR)$(bashcompdir)

TARGET_STATIC_NONDEBUG=$(TARGET).static-nondebug
TARGET_STATIC_DEBUG=$(TARGET).static-debug

LIBSDIR=$(TOP)/src
LIBSDIR_DEBUG=$(LIBSDIR)/debug
LIBSDIR_NONDEBUG=$(LIBSDIR)/nondebug

ifneq ($(DEBUG),)
LIBSDIR_PRIV=$(LIBSDIR_DEBUG)
else
LIBSDIR_PRIV=$(LIBSDIR_NONDEBUG)
endif

PMEMLOG_PRIV_OBJ=$(LIBSDIR_PRIV)/libpmemlog/libpmemlog_unscoped.o
PMEMOBJ_PRIV_OBJ=$(LIBSDIR_PRIV)/libpmemobj/libpmemobj_unscoped.o
PMEMBLK_PRIV_OBJ=$(LIBSDIR_PRIV)/libpmemblk/libpmemblk_unscoped.o

LIBS += -pthread

ifeq ($(LIBPMEMPOOL), y)
DYNAMIC_LIBS += -lpmempool
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmempool.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmempool.a
endif

ifeq ($(LIBPMEMBLK), y)
DYNAMIC_LIBS += -lpmemblk
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemblk.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemblk.a
endif

ifeq ($(LIBPMEMLOG), y)
DYNAMIC_LIBS += -lpmemlog
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemlog.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemlog.a
endif

ifeq ($(LIBPMEMOBJ), y)
DYNAMIC_LIBS += -lpmemobj
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemobj.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemobj.a
endif

ifeq ($(LIBPMEM),y)
DYNAMIC_LIBS += -lpmem
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmem.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmem.a
endif

ifeq ($(LIBVMEM),y)
DYNAMIC_LIBS += -lvmem
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libvmem.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libvmem.a
endif

ifeq ($(TOOLS_COMMON), y)
vpath %.c $(TOP)/src/common
vpath %.c $(TOP)/src/tools/pmempool

OBJS += common.o output.o util.o util_linux.o set.o set_linux.o
CFLAGS += -I$(TOP)/src/common
CFLAGS += -I$(TOP)/src/libpmemlog
CFLAGS += -I$(TOP)/src/libpmemblk
CFLAGS += -I$(TOP)/src/libpmemobj
CFLAGS += -I$(TOP)/src/tools/pmempool
common.o: CFLAGS += -D__USE_UNIX98

endif

ifneq ($(LIBPMEMLOG_PRIV),)
OBJS += pmemlog_priv.o
endif

ifneq ($(LIBPMEMOBJ_PRIV),)
OBJS += pmemobj_priv.o
endif

ifneq ($(LIBPMEMBLK_PRIV),)
OBJS += pmemblk_priv.o
endif

MAKEFILE_DEPS=$(TOP)/src/tools/Makefile.inc $(TOP)/src/common.inc

ifneq ($(TARGET),)
all: $(TARGET) $(TARGET_STATIC_NONDEBUG) $(TARGET_STATIC_DEBUG)
else
all:
endif

clean:
	$(RM) $(OBJS) $(CLEAN_FILES)

clobber: clean
ifneq ($(TARGET),)
	$(RM) $(TARGET)
	$(RM) $(TARGET_STATIC_NONDEBUG)
	$(RM) $(TARGET_STATIC_DEBUG)
	$(RM) -r .deps
endif

install: all
ifeq ($(INSTALL_TARGET),y)
ifneq ($(TARGET),)
	install -d $(TARGET_DIR)
	install -p -m 0755 $(TARGET) $(TARGET_DIR)
endif
ifneq ($(BASH_COMP_FILES),)
	install -d $(BASH_COMP_DESTDIR)
	install -p -m 0644 $(BASH_COMP_FILES) $(BASH_COMP_DESTDIR)
endif
endif

uninstall:
ifeq ($(INSTALL_TARGET),y)
ifneq ($(TARGET),)
	$(RM) $(TARGET_DIR)/$(TARGET)
endif
ifneq ($(BASH_COMP_FILES),)
	$(RM) $(BASH_COMP_DESTDIR)/$(BASH_COMP_FILES)
endif
endif

%.gz: %
	gzip -c ./$< > $@

%.txt: %
	man ./$< > $@

$(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG): $(OBJS) $(MAKEFILE_DEPS)

$(TARGET_STATIC_DEBUG): $(STATIC_DEBUG_LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(STATIC_DEBUG_LIBS) $(LIBS)

$(TARGET_STATIC_NONDEBUG): $(STATIC_NONDEBUG_LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(STATIC_NONDEBUG_LIBS) $(LIBS)

$(TARGET):
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) $(DYNAMIC_LIBS)

$(PMEMLOG_PRIV_OBJ):
	$(MAKE) -C $(LIBSDIR) libpmemlog

pmemlog_priv.o: $(PMEMLOG_PRIV_OBJ)
	$(OBJCOPY) --localize-hidden $(addprefix -G, $(LIBPMEMLOG_PRIV)) $< $@

$(PMEMOBJ_PRIV_OBJ):
	$(MAKE) -C $(LIBSDIR) libpmemobj

pmemobj_priv.o: $(PMEMOBJ_PRIV_OBJ)
	$(OBJCOPY) --localize-hidden $(addprefix -G, $(LIBPMEMOBJ_PRIV)) $< $@

$(PMEMBLK_PRIV_OBJ):
	$(MAKE) -C $(LIBSDIR) libpmemblk

pmemblk_priv.o: $(PMEMBLK_PRIV_OBJ)
	$(OBJCOPY) --localize-hidden $(addprefix -G, $(LIBPMEMBLK_PRIV)) $< $@

objdir=.

%.o: %.c $(MAKEFILE_DEPS)
	@mkdir -p .deps
	$(CC) -MD $(CFLAGS) $(INCS) -c -o $@ $<
	$(create-deps)

test check pcheck: all

.PHONY: all clean clobber install uninstall test check pcheck

-include .deps/*.P
