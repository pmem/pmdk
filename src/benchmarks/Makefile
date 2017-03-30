#
# Copyright 2014-2017, Intel Corporation
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
# src/benchmarks/Makefile -- build all benchmarks
#
TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..

include $(TOP)/src/common.inc

vpath %.c $(TOP)/src/examples/libpmemobj/tree_map
vpath %.c $(TOP)/src/examples/libpmemobj/map
vpath %.c $(TOP)/src/examples/libpmemobj/hashmap

vpath %.c $(TOP)/src/libpmemobj
vpath %.c $(TOP)/src/common
vpath %.c $(TOP)/src/librpmem
vpath %.c $(TOP)/src/rpmem_common


BENCHMARK = pmembench

GLIB:= $(call check_package, glib-2.0)
ifeq ($(GLIB),y)
TARGET = $(BENCHMARK)
else
$(info NOTE: Skipping pmembench because glib-2.0 is missing \
-- see src/benchmarks/README for details.)
endif

all: $(TARGET)

SRC=pmembench.cpp\
    benchmark_time.cpp\
    benchmark_worker.cpp\
    clo.cpp\
    clo_vec.cpp\
    config_reader.cpp\
    scenario.cpp\
    log.cpp\
    blk.cpp\
    vmem.cpp\
    pmem_memset.cpp\
    pmem_memcpy.cpp\
    pmem_flush.cpp\
    pmemobj_gen.cpp\
    pmemobj_persist.cpp\
    obj_pmalloc.cpp\
    obj_locks.cpp\
    obj_lanes.cpp\
    map_bench.cpp\
    pmemobj_tx.cpp\
    pmemobj_atomic_lists.cpp

# Configuration file without the .cfg extension
CONFIGS=pmembench_log\
	pmembench_blk\
	pmembench_vmem\
	pmembench_memset\
	pmembench_memcpy\
	pmembench_flush\
	pmembench_obj_pmalloc\
	pmembench_obj_persist\
	pmembench_obj_gen\
	pmembench_obj_locks\
	pmembench_obj_lanes\
	pmembench_map\
	pmembench_tx\
	pmembench_atomic_lists

OBJS=$(SRC:.cpp=.o)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif
LDFLAGS = -L$(LIBS_PATH)
LDFLAGS += -L../examples/libpmemobj/map
LDFLAGS += $(EXTRA_LDFLAGS)
ifeq ($(DEBUG),)
LIBS += ../nondebug/libpmemcommon.a
else
LIBS += ../debug/libpmemcommon.a
endif
LIBS += -lpmemobj -lpmemlog -lpmemblk -lpmem -lvmem -pthread -lm -ldl
ifeq ($(call check_librt), n)
LIBS += -lrt
endif

CXXFLAGS = -Wall
CXXFLAGS += -Werror
CXXFLAGS += -std=c++0x
CXXFLAGS += -Wno-invalid-offsetof
CXXFLAGS += -Wpointer-arith
CXXFLAGS += -Wunused-macros
CXXFLAGS += -pthread
CXXFLAGS += -I../include
CXXFLAGS += -I../libpmemobj
CXXFLAGS += -I../common
CXXFLAGS += -I../examples/libpmemobj/map
CXXFLAGS += -I../rpmem_common
CXXFLAGS += -I../librpmem
CXXFLAGS += -DSRCVERSION='"$(SRCVERSION)"'

ifeq ($(BUILD_RPMEM),y)
SRC += rpmem_persist.cpp
LIBS += -lrpmem
CONFIGS += pmembench_rpmem
CXXFLAGS += -DRPMEM_AVAILABLE
else
$(info NOTE: Skipping librpmem benchmark because libfabric is missing \
-- see src/librpmem/README for details.)
endif

ifeq ($(GLIB),y)
CXXFLAGS += $(shell $(PKG_CONFIG) --cflags glib-2.0)
LIBS += $(shell $(PKG_CONFIG) --libs glib-2.0)
endif

LIBMAP_DIR=../examples/libpmemobj/map
LIBMAP=$(LIBMAP_DIR)/libmap.a

OBJS += pmemobj.o

ifeq ($(DEBUG),)
CXXFLAGS += -O3
LIBS_PATH=../nondebug
else
CXXFLAGS += -ggdb
LIBS_PATH=../debug
endif

GLIB_TEST_PROG="\#include <glib.h>\nint main(){return 0;}"
GLIB_SILENCE := $(shell printf $(GLIB_TEST_PROG) |\
	$(CXX) $(CXXFLAGS) -x c -o /dev/null - 2>/dev/null && echo n || echo y)

ifeq ($(GLIB_SILENCE), y)
CXXFLAGS += -Wno-unknown-attributes
endif

CXXFLAGS += $(EXTRA_CXXFLAGS)

objdir=.

%.o: %.cpp Makefile
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CXX) -MD -c -o $@ $(CXXFLAGS) $<
	$(call check-os, $@, $<)
	$(create-deps)

%.htmp: %.h
	$(call check-cstyle, $<, $@)

%.hpptmp: %.hpp
	$(call check-cstyle, $<, $@)

$(BENCHMARK): $(TMP_HEADERS) $(OBJS) $(LIBMAP)
	$(CXX) -o $@ $(LDFLAGS) $(OBJS) $(LIBMAP) $(LIBS)

$(LIBMAP):
	$(MAKE) -C $(LIBMAP_DIR) map

clean:
	$(RM) $(OBJS) $(TMP_HEADERS)
	$(RM) *.csv

clobber: clean
	$(RM) $(BENCHMARK)
	$(RM) *.csv
	$(RM) -r .deps

$(CONFIGS):
	LD_LIBRARY_PATH=$(LIBS_PATH) ./$(BENCHMARK) $@.cfg > $@.csv

run: $(BENCHMARK) $(CONFIGS)

sparse:

.PHONY: all clean clobber run $(CONFIGS)

PMEMOBJ_SYMBOLS=pmalloc pfree lane_hold lane_release

pmemobj.o: $(LIBS_PATH)/libpmemobj/libpmemobj_unscoped.o
	objcopy --localize-hidden $(addprefix -G, $(PMEMOBJ_SYMBOLS)) $< $@

-include .deps/*.P
