# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

#
# src/test/pmem2_granularity/Makefile -- build pmem2_granularity test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_granularity

LIBPMEM2=internal-debug

OBJS += pmem2_granularity.o\
	ut_pmem2_config.o\
	ut_pmem2_utils.o\
	mocks_posix.o

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, mocks_posix.c)
