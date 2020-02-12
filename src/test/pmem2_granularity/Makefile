# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/pmem2_granularity/Makefile -- build pmem2_granularity test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_granularity

OBJS += pmem2_granularity.o\
	ut_pmem2_config.o\
	ut_pmem2_utils.o\
	mocks_posix.o

LIBPMEM2=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, mocks_posix.c)
