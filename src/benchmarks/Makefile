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
# src/benchmarks/Makefile -- build all benchmarks
#
TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..

include $(TOP)/src/common.inc

vpath %.c $(TOP)/src/examples/libpmemobj/tree_map
vpath %.c $(TOP)/src/examples/libpmemobj/map
vpath %.c $(TOP)/src/examples/libpmemobj/hashmap

vpath %.c $(TOP)/src/libpmemobj
vpath %.c $(TOP)/src/common


BENCHMARK = pmembench

GLIB:= $(call check_package, glib-2.0)
ifeq ($(GLIB),y)
TARGET = $(BENCHMARK)
else
$(info NOTE: Skipping pmembench because glib-2.0 is missing \
-- see src/benchmarks/README for details.)
endif

all: $(TARGET)

SRC=pmembench.c\
    benchmark_time.c\
    benchmark_worker.c\
    util.c\
    util_linux.c\
    out.c\
    set.c\
    set_linux.c\
    clo.c\
    clo_vec.c\
    config_reader.c\
    scenario.c\
    log.c\
    blk.c\
    vmem.c\
    pmem_memset.c\
    pmem_memcpy.c\
    pmem_flush.c\
    pmemobj_gen.c\
    obj_pmalloc.c\
    obj_locks.c\
    obj_lanes.c\
    map_bench.c\
    pmemobj_tx.c\
    pmemobj_atomic_lists.c

# Configuration file without the .cfg extension
CONFIGS=pmembench_log\
	pmembench_blk\
	pmembench_vmem\
	pmembench_memset\
	pmembench_memcpy\
	pmembench_flush\
	pmembench_obj_pmalloc\
	pmembench_obj_gen\
	pmembench_obj_locks\
	pmembench_obj_lanes\
	pmembench_map\
	pmembench_tx\
	pmembench_atomic_lists

OBJS=$(SRC:.c=.o)
LDFLAGS = -L$(LIBS_PATH)
LDFLAGS += -L../examples/libpmemobj/map
LDFLAGS += $(EXTRA_LDFLAGS)
LIBS += -lpmemobj -lpmemlog -lpmemblk -lpmem -lvmem -pthread -lm
ifeq ($(call check_librt), n)
LIBS += -lrt
endif
CFLAGS  = -std=gnu99
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith
CFLAGS += -Wunused-macros
CFLAGS += -pthread
CFLAGS += -I../include
CFLAGS += -I../libpmemobj
CFLAGS += -I../common
CFLAGS += -I../examples/libpmemobj/map
CFLAGS += -DSRCVERSION='"$(SRCVERSION)"'
ifeq ($(GLIB),y)
CFLAGS += $(shell $(PKG_CONFIG) --cflags glib-2.0)
LIBS += $(shell $(PKG_CONFIG) --libs glib-2.0)
endif

LIBMAP_DIR=../examples/libpmemobj/map
LIBMAP=$(LIBMAP_DIR)/libmap.a

OBJS += pmemobj.o

ifeq ($(DEBUG),)
CFLAGS += -O3
LIBS_PATH=../nondebug
else
CFLAGS += -ggdb
LIBS_PATH=../debug
endif

GLIB_TEST_PROG="\#include <glib.h>\nint main(){return 0;}"
GLIB_SILENCE := $(shell printf $(GLIB_TEST_PROG) |\
	$(CC) $(CFLAGS) -x c -o /dev/null - 2>/dev/null && echo n || echo y)

ifeq ($(GLIB_SILENCE), y)
CFLAGS += -Wno-unknown-attributes
endif

CFLAGS += $(EXTRA_CFLAGS)

objdir=.

%.o: %.c
	@mkdir -p .deps
	$(CC) -MD -c -o $@ $(CFLAGS) $<
	$(create-deps)

$(BENCHMARK): $(OBJS) $(LIBMAP)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

$(LIBMAP):
	$(MAKE) -C $(LIBMAP_DIR) map

clean:
	$(RM) $(OBJS)
	$(RM) *.csv

clobber: clean
	$(RM) $(BENCHMARK)
	$(RM) *.csv
	$(RM) -r .deps

$(CONFIGS):
	LD_LIBRARY_PATH=$(LIBS_PATH) ./$(BENCHMARK) $@.cfg > $@.csv

run: $(BENCHMARK) $(CONFIGS)

.PHONY: all clean clobber run $(CONFIGS)

PMEMOBJ_SYMBOLS=pmalloc pfree lane_hold lane_release

pmemobj.o: $(LIBS_PATH)/libpmemobj/libpmemobj_unscoped.o
	objcopy --localize-hidden $(addprefix -G, $(PMEMOBJ_SYMBOLS)) $< $@

-include .deps/*.P
