# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmemset_source/Makefile -- build for pmemset source API unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmemset

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_source
OBJS += pmemset_source.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc
