# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmemset_new/Makefile -- build pmemset_new unit test
#

TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmemset_new
OBJS += pmemset_new.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug
LIBPMEMCOMMON=y
include ../Makefile.inc
