# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

#
# src/test/pmemset_deep_flush/Makefile -- build for pmemset_deep_flush API unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmemset

vpath %.c $(TOP)/src/libpmem2
vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2
INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_deep_flush
OBJS += pmemset_deep_flush.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug
LIBPMEM2=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, pmemset_deep_flush.c)
