# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmem2_map_from_existing/Makefile -- build pmem2_map unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_map_from_existing
OBJS += pmem2_map_from_existing.o\
	ut_pmem2_utils.o\
	ut_pmem2_source.o

LIBPMEM2=internal-debug
LIBPMEMCORE=y
include ../Makefile.inc
