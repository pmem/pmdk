# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_memcpy/Makefile -- build pmem2_memcpy test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/unittest
INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_memcpy
OBJS += pmem2_memcpy.o\
	memcpy_common.o\
	ut_pmem2_utils.o\
	ut_pmem2_config.o\
	ut_pmem2_source.o

LIBPMEM2=y

include ../Makefile.inc
