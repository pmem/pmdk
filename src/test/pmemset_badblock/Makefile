# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/pmemset_badblock/Makefile -- build pmemset_badblock test
#

TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_badblock
OBJS += pmemset_badblock.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc
