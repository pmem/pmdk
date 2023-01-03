# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/unittest/Makefile -- build unittest support library
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../..
include $(TOP)/src/common.inc

vpath %.c $(TOP)/src/common
vpath %.h $(TOP)/src/common

vpath %.c $(TOP)/src/core
vpath %.h $(TOP)/src/core

TARGET_UT = libut.a
OBJS_UT = ut.o ut_alloc.o ut_file.o ut_pthread.o ut_signal.o ut_backtrace.o\
	os_posix.o os_thread_posix.o util.o alloc.o rand.o ut_fh.o

TARGET_UT_PMEM2 = ut_pmem2.a
OBJS_UT_PMEM2 = ut_pmem2_config.o ut_pmem2_utils.o

CFLAGS = -I$(TOP)/src/include
CFLAGS += -I$(TOP)/src/common
CFLAGS += -I$(TOP)/src/core
CFLAGS += -I$(TOP)/src/libpmem2
CFLAGS += $(OS_INCS)
CFLAGS += -std=gnu99
CFLAGS += -ggdb
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith
CFLAGS += -Wsign-conversion
CFLAGS += -Wsign-compare
ifeq ($(WCONVERSION_AVAILABLE), y)
CFLAGS += -Wconversion
endif
CFLAGS += -pthread
CFLAGS += -fno-common

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

ifeq ($(USE_LIBUNWIND),y)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libunwind) -DUSE_LIBUNWIND
endif

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

CFLAGS += $(EXTRA_CFLAGS)

LIBS += $(LIBUTIL)

all test: $(TARGET_UT) $(TARGET_UT_PMEM2)

$(TARGET_UT): $(OBJS_UT)
	$(AR) rv $@ $(OBJS_UT)

$(TARGET_UT_PMEM2): $(OBJS_UT_PMEM2)
	$(AR) rv $@ $(OBJS_UT_PMEM2)

ifneq ($(CSTYLEON),0)
$(TARGET): unittest.htmp
endif

objdir=.

.c.o:
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CC) -MD -c $(CFLAGS) $(INCS) $(COMMONINCS) $(call coverage-path, $<) -o $@
	$(create-deps)

%.htmp: %.h
	$(call check-cstyle, $<, $@)

clean:
	$(RM) *.o core a.out unittest.htmp

clobber: clean
	$(RM) $(TARGET_UT) $(TARGET_UT_PMEM2)
	$(RM) -r .deps

test check pcheck pycheck: all

sparse:
	$(sparse-c)

.PHONY: all test check clean clobber cstyle format pcheck

-include .deps/*.P
