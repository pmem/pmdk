# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/pmem2_badblock/Makefile -- build pmem2_badblock test
#

TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_badblock
OBJS += pmem2_badblock.o\
	ut_pmem2_utils.o

LIBPMEM2=internal-debug

include ../Makefile.inc
