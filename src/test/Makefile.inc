#
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

#
# src/test/Makefile.inc -- common Makefile defs for unit tests
#
# These defaults apply to most unit tests.  The individual Makefile
# for each unit test overrides the defaults as necessary.
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..

include $(TOP)/src/common.inc

LIBS_DIR=$(TOP)/src

EXAMPLES_DIR=$(TOP)/src/examples
EX_LIBPMEM=$(EXAMPLES_DIR)/libpmem
EX_LIBPMEMBLK=$(EXAMPLES_DIR)/libpmemblk
EX_LIBPMEMLOG=$(EXAMPLES_DIR)/libpmemlog
EX_LIBPMEMOBJ=$(EXAMPLES_DIR)/libpmemobj

UT = ../unittest/libut.a
LIBS += $(UT)

ifeq ($(USE_LIBUNWIND),1)
LIBS += -ldl -lunwind
else
ifneq ($(USE_LIBUNWIND),0)
UNWIND := $(call check_package, libunwind)
ifeq ($(UNWIND),y)
LIBS += -ldl $(shell $(PKG_CONFIG) --libs libunwind)
endif
endif
endif

LIBS += -L$(LIBS_DIR)/debug
LIBS += -pthread

ifeq ($(LIBPMEMPOOL), y)
DYNAMIC_LIBS += -lpmempool
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmempool.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmempool.a
endif

ifeq ($(LIBPMEMBLK), y)
DYNAMIC_LIBS += -lpmemblk
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemblk.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemblk.a
endif

ifeq ($(LIBPMEMLOG), y)
DYNAMIC_LIBS += -lpmemlog
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemlog.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemlog.a
endif

ifeq ($(LIBPMEMOBJ), y)
DYNAMIC_LIBS += -lpmemobj
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemobj.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemobj.a
endif

ifeq ($(LIBPMEM),y)
DYNAMIC_LIBS += -lpmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmem.a
endif

ifeq ($(LIBVMEM),y)
DYNAMIC_LIBS += -lvmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libvmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libvmem.a
endif

TEST_TYPE = check
TEST_BUILD = all
TEST_FS = all
TEST_TIME = 3m
MEMCHECK = auto
PMEMCHECK = auto
HELGRIND = auto
DRD = auto

#
# This is a helper function to be combined with usage of macros available
# in the unittest framework. It scans the code for functions that should be
# wrapped and adds required linker flags.
#
WRAPPER_FUNCS=FUNC_MOCK_RET_ALWAYS|FUNC_MOCK
PAREN=(
extract_funcs = $(shell \
    grep -Po '($(WRAPPER_FUNCS))\$(PAREN)\K[^,]*' $(1) | \
    awk '{print "-Wl,--wrap="$$0}')

INCS = -I../unittest -I$(TOP)/src/include

COMMON_FLAGS  = -ggdb
COMMON_FLAGS += -Wall
COMMON_FLAGS += -Werror
COMMON_FLAGS += -Wpointer-arith
COMMON_FLAGS += -Wunused-macros
COMMON_FLAGS += -fno-common

CXXFLAGS  = -std=c++11
CXXFLAGS += -ggdb
CXXFLAGS += $(COMMON_FLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)

CFLAGS  = -std=gnu99
CFLAGS += -Wmissing-prototypes
CFLAGS += $(COMMON_FLAGS)
CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS = -Wl,--warn-common -Wl,--fatal-warnings $(EXTRA_LDFLAGS)

LINKER=$(CC)
ifeq ($(COMPILE_LANG), cpp)
LINKER=$(CXX)
endif

#
# By default debug and non-debug static versions are built.
# It can be changed by setting BUILD_STATIC_DEBUG
# or BUILD_STATIC_NONDEBUG to 'n'.
#
ifneq ($(TARGET),)
ifneq ($(BUILD_STATIC_DEBUG),n)
TARGET_STATIC_DEBUG=$(TARGET).static-debug
endif
ifneq ($(BUILD_STATIC_NONDEBUG),n)
TARGET_STATIC_NONDEBUG=$(TARGET).static-nondebug
endif
endif

MAKEFILE_DEPS=Makefile ../Makefile.inc $(TOP)/src/common.inc

all: $(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG)

$(UT):
	$(MAKE) -C ../unittest

$(TARGET_STATIC_DEBUG): $(OBJS) $(UT) $(STATIC_DEBUG_LIBS) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(STATIC_DEBUG_LIBS) $(LIBS)

$(TARGET_STATIC_NONDEBUG): $(OBJS) $(UT) $(STATIC_NONDEBUG_LIBS) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(STATIC_NONDEBUG_LIBS) $(LIBS)

$(TARGET): $(OBJS) $(UT) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(DYNAMIC_LIBS) $(LIBS)

objdir=.

%.o: %.c $(MAKEFILE_DEPS)
	@mkdir -p .deps
	$(CC) -MD -c $(CFLAGS) $(INCS) $< -o $@
	$(create-deps)

%.o: %.cpp $(MAKEFILE_DEPS)
	@mkdir -p .deps
	$(CXX) -MD -c $(CXXFLAGS) $(INCS) $< -o $@
	$(create-deps)

clean:
	$(RM) *.o */*.o core a.out *.log testfile*

clobber: clean
	$(RM) $(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG)
	$(RM) -r .deps

TST=$(shell basename `pwd`)
TSTCHECKS=$(shell ls -1 TEST* | grep -v -i -e "\.ps1" | sort -V)

$(TSTCHECKS): all
	@cd .. && ./RUNTESTS ${TST} -b $(TEST_BUILD) -t $(TEST_TYPE) -f $(TEST_FS) -o $(TEST_TIME) -m $(MEMCHECK) -p $(PMEMCHECK) -e $(HELGRIND) -d $(DRD) -s $@

check: all
	@cd .. && ./RUNTESTS ${TST} -b $(TEST_BUILD) -t $(TEST_TYPE) -f $(TEST_FS) -o $(TEST_TIME) -m $(MEMCHECK) -p $(PMEMCHECK) -e $(HELGRIND) -d $(DRD)

pcheck: export NOTTY=1

pcheck: $(TSTCHECKS)

test: all

TOOLS=../tools

$(TOOLS)/pmemspoil/pmemspoil:
	$(MAKE) -C $(TOOLS)/pmemspoil all

ifeq ($(USE_PMEMSPOIL), y)
all: $(TOOLS)/pmemspoil/pmemspoil
endif

$(TOOLS)/bttcreate/bttcreate:
	$(MAKE) -C $(TOOLS)/bttcreate all

ifeq ($(USE_BTTCREATE), y)
all: $(TOOLS)/bttcreate/bttcreate
endif

$(TOOLS)/pmemwrite/pmemwrite:
	$(MAKE) -C $(TOOLS)/pmemwrite all

ifeq ($(USE_PMEMWRITE), y)
all: $(TOOLS)/pmemwrite/pmemwrite
endif

$(TOOLS)/pmemalloc/pmemalloc:
	$(MAKE) -C $(TOOLS)/pmemalloc all

ifeq ($(USE_PMEMALLOC), y)
all: $(TOOLS)/pmemalloc/pmemalloc
endif

$(TOOLS)/pmemdetect/pmemdetect.static-nondebug:
	$(MAKE) -C $(TOOLS)/pmemdetect all

all: $(TOOLS)/pmemdetect/pmemdetect.static-nondebug

.PHONY: all check clean clobber pcheck test $(TSTCHECKS)

-include .deps/*.P
