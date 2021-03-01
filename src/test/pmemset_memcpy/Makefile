# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/pmemset_memcpy/Makefile -- build pmemset_memcpy test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/test/pmem2_memcpy
INCS += -I$(TOP)/src/libpmem2

TARGET = pmemset_memcpy
OBJS += pmemset_memcpy.o\
	memcpy_common.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_memcpy
