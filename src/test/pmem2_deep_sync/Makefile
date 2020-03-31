# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_deep_sync/Makefile -- build pmem2_deep_sync test
#
TOP = ../../..
vpath %.c $(TOP)/src/libpmem2

INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_deep_sync

OBJS += pmem2_deep_sync.o\
	deep_sync.o\
	mocks_posix.o

LIBPMEM2=internal-debug

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, mocks_posix.c)
