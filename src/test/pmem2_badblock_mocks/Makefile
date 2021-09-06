# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_badblock_mocks/Makefile --
#           -- build pmem2_badblock_mocks test
#

TOP = ../../..
vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_badblock_mocks

OBJS += pmem2_badblock_mocks.o\
	mocks_ndctl.o\
	mocks_pmem2.o\
	mocks_other.o\
	ut_pmem2_utils.o

LIBPMEM2=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, mocks_ndctl.c)
LDFLAGS += $(call extract_funcs, mocks_pmem2.c)
LDFLAGS += $(call extract_funcs, mocks_other.c)
