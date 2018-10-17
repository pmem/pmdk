#
# Copyright 2014-2018, Intel Corporation
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

INCS += $(OS_INCS)
LDFLAGS += $(OS_LIBS)
LIBS_DIR=$(TOP)/src

EXAMPLES_DIR=$(TOP)/src/examples

UT = ../unittest/libut.a
LIBS += $(UT) $(LIBUUID)

ifeq ($(USE_LIBUNWIND),y)
LIBS += $(LIBDL) $(LIBUNWIND_LIBS)
endif

LIBS += -L$(LIBS_DIR)/debug
LIBS += -pthread $(LIBUTIL)

ifeq ($(LIBRT_NEEDED), y)
LIBS += -lrt
endif

ifeq ($(LIBPMEMCOMMON), y)
LIBPMEM=y
OBJS += $(LIBS_DIR)/debug/libpmemcommon.a
INCS += -I$(TOP)/src/common
endif

ifeq ($(LIBPMEMCOMMON), internal-nondebug)
OBJS +=\
	$(TOP)/src/nondebug/common/file.o\
	$(TOP)/src/nondebug/common/file_posix.o\
	$(TOP)/src/nondebug/common/mmap.o\
	$(TOP)/src/nondebug/common/mmap_posix.o\
	$(TOP)/src/nondebug/common/os_posix.o\
	$(TOP)/src/nondebug/common/os_thread_posix.o\
	$(TOP)/src/nondebug/common/out.o\
	$(TOP)/src/nondebug/common/pool_hdr.o\
	$(TOP)/src/nondebug/common/util.o\
	$(TOP)/src/nondebug/common/util_posix.o\
#	$(TOP)/src/nondebug/common/uuid.o\
#	$(call osdep, $(TOP)/src/nondebug/common/uuid,.o)

INCS += -I$(TOP)/src/common
endif

ifeq ($(LIBPMEMCOMMON), internal-debug)
#vpath %.c $(TOP)/src/common
#vpath %.h $(TOP)/src/common

OBJS +=\
	$(TOP)/src/debug/common/file.o\
	$(TOP)/src/debug/common/file_posix.o\
	$(TOP)/src/debug/common/mmap.o\
	$(TOP)/src/debug/common/mmap_posix.o\
	$(TOP)/src/debug/common/os_posix.o\
	$(TOP)/src/debug/common/os_thread_posix.o\
	$(TOP)/src/debug/common/out.o\
	$(TOP)/src/debug/common/pool_hdr.o\
	$(TOP)/src/debug/common/util.o\
	$(TOP)/src/debug/common/util_posix.o\
#	$(TOP)/src/debug/common/uuid.o\
#	$(call osdep, $(TOP)/src/debug/common/uuid,.o)

INCS += -I$(TOP)/src/common
endif

ifeq ($(LIBVMEM),y)
DYNAMIC_LIBS += -lvmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libvmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libvmem.a
endif

ifneq ($(LIBPMEMCOMMON)$(LIBVMEM),)
LIBS += -pthread
endif

#
# This is a helper function to be combined with usage of macros available
# in the unittest framework. It scans the code for functions that should be
# wrapped and adds required linker flags.
#
PAREN=(
extract_funcs = $(shell \
    awk -F '[$(PAREN),]' \
    '/(FUNC_MOCK_RET_ALWAYS|FUNC_MOCK_RET_ALWAYS_VOID|FUNC_MOCK)\$(PAREN)[^,]/ \
    { \
        print "-Wl,--wrap=" $$2 \
    }' $(1) )

INCS += -I../unittest -I$(TOP)/src/include -I$(TOP)/src/common

COMMON_FLAGS  = -ggdb
COMMON_FLAGS += -Wall
COMMON_FLAGS += -Werror
COMMON_FLAGS += -Wpointer-arith

ifeq ($(IS_ICC), n)
COMMON_FLAGS += -Wunused-macros
endif
COMMON_FLAGS += -fno-common

CXXFLAGS  = -std=c++11
CXXFLAGS += $(GLIBC_CXXFLAGS)
CXXFLAGS += -ggdb
CXXFLAGS += $(COMMON_FLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)

CFLAGS  = -std=gnu99
CFLAGS += -Wmissing-prototypes
CFLAGS += $(COMMON_FLAGS)

ifneq ($(USING_JEMALLOC_HEADERS),y)
CFLAGS += -Wsign-conversion
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

ifeq ($(WCAST_FUNCTION_TYPE_AVAILABLE), y)
CFLAGS += -Wcast-function-type
endif

CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS = -Wl,--warn-common -Wl,--fatal-warnings $(EXTRA_LDFLAGS)

ifeq ($(OS_DIMM),ndctl)
CFLAGS += -DSDS_ENABLED
endif

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
CXXFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

ifeq ($(VALGRIND),0)
CFLAGS += -DVALGRIND_ENABLED=0
CXXFLAGS += -DVALGRIND_ENABLED=0
endif

ifneq ($(SANITIZE),)
CFLAGS += -fsanitize=$(SANITIZE)
CXXFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
endif

LINKER=$(CC)
ifeq ($(COMPILE_LANG), cpp)
LINKER=$(CXX)
endif

ifneq ($(TARGET),)
SCP_TARGET=$(TARGET)
SCP_SRC_DIR=.
#
# By default debug and non-debug static versions are built.
# It can be changed by setting BUILD_STATIC_DEBUG, BUILD_STATIC_NONDEBUG
# or BUILD_STATIC (for both of them) to 'n'.
#
ifneq ($(BUILD_STATIC),n)
ifneq ($(BUILD_STATIC_DEBUG),n)
TARGET_STATIC_DEBUG=$(TARGET).static-debug
SCP_TARGET_STATIC_DEBUG=$(SCP_SRC_DIR)/$(SCP_TARGET).static-debug
endif
ifneq ($(BUILD_STATIC_NONDEBUG),n)
ifneq ($(DEBUG),1)
TARGET_STATIC_NONDEBUG=$(TARGET).static-nondebug
SCP_TARGET_STATIC_NONDEBUG=$(SCP_SRC_DIR)/$(SCP_TARGET).static-nondebug
endif
endif
endif
endif

SCP=../sync-remotes/copy-to-remote-nodes.sh
TESTCONFIG=../testconfig.sh
SYNC_FILE=.synced

MAKEFILE_DEPS=Makefile ../Makefile.inc $(TOP)/src/common.inc

ifneq ($(HEADERS),)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif
endif

all: $(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG)

$(UT):
	$(MAKE) -C ../unittest

$(TARGET_STATIC_DEBUG): $(TMP_HEADERS) $(OBJS) $(UT) $(STATIC_DEBUG_LIBS) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(STATIC_DEBUG_LIBS) $(LIBS)

$(TARGET_STATIC_NONDEBUG): $(TMP_HEADERS) $(OBJS) $(UT) $(STATIC_NONDEBUG_LIBS) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(STATIC_NONDEBUG_LIBS) $(LIBS)

$(TARGET): $(TMP_HEADERS) $(OBJS) $(UT) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(DYNAMIC_LIBS) $(LIBS)

objdir=.

%.o: %.c $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CC) -MD -c $(CFLAGS) $(INCS) $(call coverage-path, $<) -o $@
	$(call check-os, $@, $<)
	$(create-deps)

%.o: %.cpp $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CXX) -MD -c $(CXXFLAGS) $(INCS) $(call coverage-path, $<) -o $@
	$(call check-os, $@, $<)
	$(create-deps)

%.htmp: %.h
	$(call check-cstyle, $<, $@)

clean:
	$(RM) *.o */*.o core *.core a.out *.log testfile* $(SYNC_FILE) $(TMP_HEADERS)

clobber: clean
	$(RM) $(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG)
	$(RM) -r .deps

$(TESTCONFIG):

$(SYNC_FILE): $(TARGET) $(TESTCONFIG)
ifeq ($(SCP_TO_REMOTE_NODES), y)
ifeq ($(SCP_TARGET),)
	$(SCP) test
else
ifeq ($(SCP_SRC_DIR),)
$(error SCP_SRC_DIR is not set)
endif
	$(SCP) common
	$(SCP) test $(SCP_SRC_DIR)/$(SCP_TARGET) $(SCP_TARGET_STATIC_DEBUG) $(SCP_TARGET_STATIC_NONDEBUG)
endif
	@touch $(SYNC_FILE)
endif

sync-test: all $(SYNC_FILE) $(TESTCONFIG)

TST=$(shell basename `pwd`)
TSTCHECKS=$(shell ls -1 TEST* 2> /dev/null | grep -v -i -e "\.ps1" | sort -V)

$(TSTCHECKS): sync-test
	@cd .. && ./RUNTESTS ${TST} $(RUNTEST_OPTIONS) -s $@

check: sync-test
	@cd .. && ./RUNTESTS ${TST} $(RUNTEST_OPTIONS)

pcheck: export NOTTY=1

pcheck: $(TSTCHECKS)

test: all

TOOLS=../tools

all:

sparse:
	$(if $(TARGET), $(sparse-c))

.PHONY: all check clean clobber pcheck test sync-test $(TSTCHECKS)

-include .deps/*.P
