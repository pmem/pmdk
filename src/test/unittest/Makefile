#
# Copyright 2014-2019, Intel Corporation
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

#
# src/test/unittest/Makefile -- build unittest support library
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../..
include $(TOP)/src/common.inc

vpath %.c $(TOP)/src/common
vpath %.h $(TOP)/src/common

TARGET = libut.a
OBJS = ut.o ut_alloc.o ut_file.o ut_pthread.o ut_signal.o ut_backtrace.o\
	os_posix.o os_thread_posix.o rand.o
CFLAGS = -I$(TOP)/src/include
CFLAGS += -I$(TOP)/src/common
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

all test: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rv $@ $(OBJS)

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
	$(RM) $(TARGET)
	$(RM) -r .deps

test check pcheck: all

sparse:
	$(sparse-c)

.PHONY: all test check clean clobber cstyle format pcheck

-include .deps/*.P
