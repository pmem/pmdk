# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_deep_flush/Makefile -- build pmem2_deep_flush test
#
TOP = ../../..

vpath %.c $(TOP)/src/libpmem2
vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_deep_flush

LIBPMEMCORE=internal-debug

OBJS += pmem2_deep_flush.o\
	deep_flush.o\
	deep_flush_linux.o\
	memops_generic.o\
	persist.o\
	errormsg.o\
	ut_pmem2_utils.o

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, pmem2_deep_flush.c)
