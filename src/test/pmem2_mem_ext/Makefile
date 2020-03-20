# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_mem_ext/Makefile -- build pmem2_mem_ext test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/unittest
INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_mem_ext
OBJS += pmem2_mem_ext.o\
	ut_pmem2_utils.o\
	ut_pmem2_config.o

LIBPMEM2=y

include ../Makefile.inc
