# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/pmemset_memcpy/Makefile -- build pmemset_memcpy test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/test/pmem2_memset
INCS += -I$(TOP)/src/libpmem2

TARGET = pmemset_memset
OBJS += pmemset_memset.o\
	memset_common.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_memset
