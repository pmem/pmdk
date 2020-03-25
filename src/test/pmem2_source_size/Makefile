# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

#
# src/test/pmem2_source_size/Makefile -- build pmem2_source_size unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmem2

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_source_size
OBJS += errormsg.o\
	pmem2_source_size.o\
	ut_pmem2_config.o\
	ut_pmem2_source.o\
	ut_pmem2_utils.o

LIBPMEMCOMMON=y
include ../Makefile.inc
