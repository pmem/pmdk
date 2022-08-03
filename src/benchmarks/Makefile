# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation

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
vpath %.c $(TOP)/src/libpmempool

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
    pmemobj_atomic_lists.cpp\
    poolset_util.cpp\
    benchmark_empty.cpp\
    pmemobj_tx_add_range.cpp

# Configuration file without the .cfg extension
CONFIGS=pmembench_log\
	pmembench_blk\
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
LDFLAGS = -L$(LIBS_PATH) $(OS_LIBS)
LDFLAGS += -L../examples/libpmemobj/map
LDFLAGS += $(EXTRA_LDFLAGS)
ifeq ($(DEBUG),)
LIBS += ../nondebug/libpmemcore.a
LIBS += ../nondebug/libpmemcommon.a
else
LIBS += ../debug/libpmemcore.a
LIBS += ../debug/libpmemcommon.a
endif
CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += -lpmemobj -lpmemlog -lpmemblk -lpmempool -lpmem -pthread -lm \
	$(LIBDL) $(LIBUUID) $(LIBNDCTL_LIBS)
ifeq ($(LIBRT_NEEDED), y)
LIBS += -lrt
endif

CXXFLAGS = -Wall
CXXFLAGS += -Werror
CXXFLAGS += -std=c++11
CXXFLAGS += -Wno-invalid-offsetof
CXXFLAGS += -Wpointer-arith
CXXFLAGS += -Wunused-macros
CXXFLAGS += -pthread
CXXFLAGS += -I../include
CXXFLAGS += -I../libpmemobj
CXXFLAGS += -I../common
CXXFLAGS += -I../core
CXXFLAGS += -I../examples/libpmemobj/map
CXXFLAGS += $(OS_INCS)
CXXFLAGS += -DSRCVERSION='"$(SRCVERSION)"'

ifeq ($(OS_DIMM),ndctl)
CXXFLAGS += -DSDS_ENABLED
endif

ifeq ($(COVERAGE),1)
CXXFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

ifneq ($(SANITIZE),)
CXXFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
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
	$(CXX) -MD -c -o $@ $(CXXFLAGS) $(call coverage-path, $<)
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
	$(OBJCOPY) --localize-hidden $(addprefix -G, $(PMEMOBJ_SYMBOLS)) $< $@

-include .deps/*.P
