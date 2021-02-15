# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/pmemset_memmove/Makefile -- build pmemset_memmove test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/test/pmem2_memmove
INCS += -I$(TOP)/src/libpmem2

TARGET = pmemset_memmove
OBJS += pmemset_memmove.o\
	memmove_common.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_memmove
