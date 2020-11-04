# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmemset_part/Makefile -- build for pmemset part API unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmemset

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_part
OBJS += pmemset_part.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug
LIBPMEM2=internal-debug

include ../Makefile.inc
