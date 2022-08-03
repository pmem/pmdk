# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation
#
# src/tools/Makefile.inc -- Makefile include for all tools
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..

include $(TOP)/src/common.inc

INSTALL_TARGET ?= y

INCS += -I.
INCS += -I$(TOP)/src/include
INCS += $(OS_INCS)
CFLAGS += -std=gnu99
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith
CFLAGS += -Wsign-conversion
CFLAGS += -Wsign-compare

ifeq ($(WCONVERSION_AVAILABLE), y)
CFLAGS += -Wconversion
endif

CFLAGS += -fno-common

CFLAGS += -DSRCVERSION='"$(SRCVERSION)"'

ifeq ($(OS_DIMM),ndctl)
CFLAGS += -DSDS_ENABLED
endif

ifeq ($(IS_ICC), n)
CFLAGS += -Wunused-macros
CFLAGS += -Wmissing-field-initializers
endif

ifeq ($(WUNREACHABLE_CODE_RETURN_AVAILABLE), y)
CFLAGS += -Wunreachable-code-return
endif

ifeq ($(WMISSING_VARIABLE_DECLARATIONS_AVAILABLE), y)
CFLAGS += -Wmissing-variable-declarations
endif

ifeq ($(WFLOAT_EQUAL_AVAILABLE), y)
CFLAGS += -Wfloat-equal
endif

ifeq ($(WSWITCH_DEFAULT_AVAILABLE), y)
CFLAGS += -Wswitch-default
endif

ifeq ($(WCAST_FUNCTION_TYPE_AVAILABLE), y)
CFLAGS += -Wcast-function-type
endif

ifeq ($(DEBUG),1)
CFLAGS += -ggdb $(EXTRA_CFLAGS_DEBUG)
else
CFLAGS += -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 $(EXTRA_CFLAGS_RELEASE)
endif

ifeq ($(VALGRIND),0)
CFLAGS += -DVALGRIND_ENABLED=0
CXXFLAGS += -DVALGRIND_ENABLED=0
endif

ifeq ($(FAULT_INJECTION),1)
CFLAGS += -DFAULT_INJECTION=1
CXXFLAGS += -DFAULT_INJECTION=1
endif

ifneq ($(SANITIZE),)
CFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
endif
LDFLAGS += $(OS_LIBS)

CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS += -Wl,-z,relro -Wl,--warn-common -Wl,--fatal-warnings $(EXTRA_LDFLAGS)
ifeq ($(DEBUG),1)
LDFLAGS += -L$(TOP)/src/debug
else
LDFLAGS += -L$(TOP)/src/nondebug
endif
TARGET_DIR=$(DESTDIR)$(bindir)
BASH_COMP_FILES ?=
BASH_COMP_DESTDIR = $(DESTDIR)$(bashcompdir)

ifneq ($(DEBUG),1)
TARGET_STATIC_NONDEBUG=$(TARGET).static-nondebug
endif
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

LIBS += $(LIBUUID)

ifeq ($(LIBRT_NEEDED), y)
LIBS += -lrt
endif

ifeq ($(TOOLS_COMMON), y)
LIBPMEMCOMMON=y
endif

ifeq ($(LIBPMEMCOMMON), y)
DYNAMIC_LIBS += -lpmemcommon
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemcommon.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemcommon.a
CFLAGS += -I$(TOP)/src/common
LIBPMEMCORE=y
endif

ifeq ($(LIBPMEMCORE), y)
DYNAMIC_LIBS += -lpmemcore
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemcore.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemcore.a
CFLAGS += -I$(TOP)/src/core
CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += $(LIBNDCTL_LIBS)
endif

ifeq ($(LIBPMEMPOOL), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmempool
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmempool.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmempool.a
endif

ifeq ($(LIBPMEMBLK), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmemblk
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemblk.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemblk.a
endif

ifeq ($(LIBPMEMLOG), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmemlog
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemlog.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemlog.a
endif

ifeq ($(LIBPMEMOBJ), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmemobj
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmemobj.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmemobj.a
endif

ifeq ($(LIBPMEM),y)
DYNAMIC_LIBS += -lpmem
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmem.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmem.a
endif

ifeq ($(LIBPMEM2),y)
DYNAMIC_LIBS += -lpmem2
STATIC_DEBUG_LIBS += $(LIBSDIR_DEBUG)/libpmem2.a
STATIC_NONDEBUG_LIBS += $(LIBSDIR_NONDEBUG)/libpmem2.a
CFLAGS += $(LIBNDCTL_CFLAGS) $(MINIASYNC_CFLAGS)
LIBS += $(LIBNDCTL_LIBS)
endif

# If any of these libraries is required, we need to link libpthread
ifneq ($(LIBPMEMCORE)$(LIBPMEMCOMMON)$(LIBPMEM)$(LIBPMEM2)$(LIBPMEMPOOL)$(LIBPMEMBLK)$(LIBPMEMLOG)$(LIBPMEMOBJ),)
LIBS += -pthread
endif

# If any of these libraries is required, we need to link libdl
ifneq ($(LIBPMEMCOMMON)$(LIBPMEMPOOL)$(LIBPMEMOBJ),)
LIBS += $(LIBDL)
endif

ifeq ($(TOOLS_COMMON), y)
vpath %.c $(TOP)/src/tools/pmempool

OBJS += common.o output.o

CFLAGS += -I$(TOP)/src/core
CFLAGS += -I$(TOP)/src/common
CFLAGS += -I$(TOP)/src/libpmemlog
CFLAGS += -I$(TOP)/src/libpmemblk
CFLAGS += -I$(TOP)/src/libpmemobj
CFLAGS += -I$(TOP)/src/tools/pmempool
CFLAGS += $(UNIX98_CFLAGS)

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

ifneq ($(HEADERS),)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif
endif

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

MAKEFILE_DEPS=$(TOP)/src/tools/Makefile.inc $(TOP)/src/common.inc

ifneq ($(TARGET),)
all: $(TARGET) $(TARGET_STATIC_NONDEBUG) $(TARGET_STATIC_DEBUG)
else
all:
endif

clean:
	$(RM) $(OBJS) $(CLEAN_FILES) $(TMP_HEADERS)

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
	gzip -nc ./$< > $@

%.txt: %
	man ./$< > $@

$(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG): $(TMP_HEADERS) $(OBJS) $(MAKEFILE_DEPS)

$(TARGET_STATIC_DEBUG): $(STATIC_DEBUG_LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(STATIC_DEBUG_LIBS) $(LIBS)

$(TARGET_STATIC_NONDEBUG): $(STATIC_NONDEBUG_LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(STATIC_NONDEBUG_LIBS) $(LIBS)

$(TARGET):
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(DYNAMIC_LIBS) $(LIBS)

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
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CC) -MD $(CFLAGS) $(INCS) -c -o $@ $(call coverage-path, $<)
	$(call check-os, $@, $<)
	$(create-deps)

%.htmp: %.h
	$(call check-cstyle, $<, $@)

test check pcheck: all

TESTCONFIG=$(TOP)/src/test/testconfig.sh

$(TESTCONFIG):

sparse:
	$(if $(TARGET), $(sparse-c))

.PHONY: all clean clobber install uninstall test check pcheck

-include .deps/*.P
