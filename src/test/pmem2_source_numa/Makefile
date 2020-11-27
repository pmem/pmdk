# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmem2_source_numa/Makefile -- build pmem2_source_numa unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmem2

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_source_numa
OBJS +=\
	pmem2_source_numa.o\
	ut_pmem2_config.o\
	ut_pmem2_source.o\
	ut_pmem2_utils.o

LIBPMEM2=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, pmem2_source_numa.c)
