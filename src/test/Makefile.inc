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
EX_LIBPMEM=$(EXAMPLES_DIR)/libpmem
EX_LIBPMEMBLK=$(EXAMPLES_DIR)/libpmemblk
EX_LIBPMEMLOG=$(EXAMPLES_DIR)/libpmemlog
EX_LIBPMEMOBJ=$(EXAMPLES_DIR)/libpmemobj
EX_LIBPMEMOBJCPP=$(EXAMPLES_DIR)/libpmemobj++
EX_LIBPMEMCTO=$(EXAMPLES_DIR)/libpmemcto

UT = ../unittest/libut.a
LIBS += $(UT) $(LIBUUID)

ifeq ($(USE_LIBUNWIND),1)
LIBS += $(LIBDL) -lunwind
else
ifneq ($(USE_LIBUNWIND),0)
UNWIND := $(call check_package, libunwind)
ifeq ($(UNWIND),y)
LIBS += $(LIBDL) $(shell $(PKG_CONFIG) --libs libunwind)
endif
endif
endif

LIBS += -L$(LIBS_DIR)/debug
LIBS += -pthread $(LIBUTIL)

# XXX: required by clock_gettime(), if glibc version < 2.17
# The os_clock_gettime() function is now in OS abstraction layer,
# linked to all the librariess, unit tests and benchmarks.
ifeq ($(call check_librt), n)
LIBS += -lrt
endif

ifeq ($(LIBPMEMPOOL), y)
DYNAMIC_LIBS += -lpmempool
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmempool.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmempool.a
LIBS += $(LIBDL)
CFLAGS += -DUSE_LIBDL
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
LIBS += $(LIBDL)
DYNAMIC_LIBS += -lpmemobj
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemobj.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemobj.a
endif

ifeq ($(LIBPMEMOBJ), internal-debug)
OBJS += $(TOP)/src/debug/libpmemobj/alloc_class.o\
	$(TOP)/src/debug/libpmemobj/bucket.o\
	$(TOP)/src/debug/libpmemobj/container_ctree.o\
	$(TOP)/src/debug/libpmemobj/container_seglists.o\
	$(TOP)/src/debug/libpmemobj/ctl.o\
	$(TOP)/src/debug/libpmemobj/ctl_global.o\
	$(TOP)/src/debug/libpmemobj/ctree.o\
	$(TOP)/src/debug/libpmemobj/cuckoo.o\
	$(TOP)/src/debug/libpmemobj/heap.o\
	$(TOP)/src/debug/libpmemobj/lane.o\
	$(TOP)/src/debug/libpmemobj/libpmemobj.o\
	$(TOP)/src/debug/libpmemobj/list.o\
	$(TOP)/src/debug/libpmemobj/memblock.o\
	$(TOP)/src/debug/libpmemobj/memops.o\
	$(TOP)/src/debug/libpmemobj/obj.o\
	$(TOP)/src/debug/libpmemobj/palloc.o\
	$(TOP)/src/debug/libpmemobj/pmalloc.o\
	$(TOP)/src/debug/libpmemobj/pvector.o\
	$(TOP)/src/debug/libpmemobj/recycler.o\
	$(TOP)/src/debug/libpmemobj/redo.o\
	$(TOP)/src/debug/libpmemobj/ringbuf.o\
	$(TOP)/src/debug/libpmemobj/sync.o\
	$(TOP)/src/debug/libpmemobj/tx.o\
	$(TOP)/src/debug/libpmemobj/stats.o

LIBS += $(LIBDL)
INCS += -I$(TOP)/src/libpmemobj
LIBPMEM=y
LIBPMEMCOMMON=y
endif

ifeq ($(LIBPMEMOBJ), internal-nondebug)
OBJS += $(TOP)/src/nondebug/libpmemobj/alloc_class.o\
	$(TOP)/src/nondebug/libpmemobj/bucket.o\
	$(TOP)/src/nondebug/libpmemobj/container_ctree.o\
	$(TOP)/src/nondebug/libpmemobj/container_seglists.o\
	$(TOP)/src/nondebug/libpmemobj/ctl.o\
	$(TOP)/src/nondebug/libpmemobj/ctl_global.o\
	$(TOP)/src/nondebug/libpmemobj/ctree.o\
	$(TOP)/src/nondebug/libpmemobj/cuckoo.o\
	$(TOP)/src/nondebug/libpmemobj/heap.o\
	$(TOP)/src/nondebug/libpmemobj/lane.o\
	$(TOP)/src/nondebug/libpmemobj/libpmemobj.o\
	$(TOP)/src/nondebug/libpmemobj/list.o\
	$(TOP)/src/nondebug/libpmemobj/memblock.o\
	$(TOP)/src/nondebug/libpmemobj/memops.o\
	$(TOP)/src/nondebug/libpmemobj/obj.o\
	$(TOP)/src/nondebug/libpmemobj/palloc.o\
	$(TOP)/src/nondebug/libpmemobj/pmalloc.o\
	$(TOP)/src/nondebug/libpmemobj/pvector.o\
	$(TOP)/src/nondebug/libpmemobj/recycler.o\
	$(TOP)/src/nondebug/libpmemobj/redo.o\
	$(TOP)/src/nondebug/libpmemobj/ringbuf.o\
	$(TOP)/src/nondebug/libpmemobj/sync.o\
	$(TOP)/src/nondebug/libpmemobj/tx.o\
	$(TOP)/src/nondebug/libpmemobj/stats.o

INCS += -I$(TOP)/src/libpmemobj
LIBPMEM=y
LIBPMEMCOMMON=y
endif

ifeq ($(LIBPMEMCTO), y)
DYNAMIC_LIBS += -lpmemcto
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemcto.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemcto.a
endif

ifeq ($(LIBPMEMCOMMON), y)
OBJS += $(LIBS_DIR)/debug/libpmemcommon.a
INCS += -I$(TOP)/src/common
LIBS += $(LIBDL)
LIBPMEM=y
endif

ifeq ($(LIBPMEM),y)
DYNAMIC_LIBS += -lpmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmem.a
endif

ifeq ($(LIBRPMEM),y)
DYNAMIC_LIBS += -lrpmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/librpmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/librpmem.a
endif

ifeq ($(LIBVMEM),y)
DYNAMIC_LIBS += -lvmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libvmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libvmem.a
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

ifeq ($(call check_compiler, icc), n)
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
CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS = -Wl,--warn-common -Wl,--fatal-warnings $(EXTRA_LDFLAGS)

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
CXXFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
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

ifeq ($(USE_LLVM_LIBCPP),1)

$(call check_defined, LIBCPP_LIBDIR LIBCPP_INCDIR, when using USE_LLVM_LIBCPP)

ifeq ($(COMPILE_LANG), cpp)
CXXFLAGS += -stdlib=libc++
INCS += -I$(LIBCPP_INCDIR)
LIBS += -L$(LIBCPP_LIBDIR) -lc++ -lc++abi
LDFLAGS += -stdlib=libc++ -Wl,-rpath,$(LIBCPP_LIBDIR)
endif
endif

ifeq ($(USE_CUSTOM_GCC),1)

$(call check_defined, GCC_LIBDIR GCC_INCDIR,\
		when using USE_CUSTOM_GCC)

ifeq ($(COMPILE_LANG), cpp)
INCS += -I$(GCC_INCDIR)
LIBS += -L$(GCC_LIBDIR)
LDFLAGS += -Wl,-rpath,$(GCC_LIBDIR)
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

sparse:
	$(if $(TARGET), $(sparse-c))

.PHONY: all check clean clobber pcheck test sync-test $(TSTCHECKS)

-include .deps/*.P
