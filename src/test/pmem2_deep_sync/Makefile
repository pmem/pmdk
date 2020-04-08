# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_deep_sync/Makefile -- build pmem2_deep_sync test
#
TOP = ../../..

vpath %.c $(TOP)/src/libpmem2

INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_deep_sync

LIBPMEMCORE=internal-debug

OBJS += pmem2_deep_sync.o\
	deep_sync.o\
	deep_sync_linux.o\
	memops_generic.o\
	persist.o

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, pmem2_deep_sync.c)
