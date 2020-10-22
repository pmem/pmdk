# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmemset_source/Makefile -- build pmemset_source unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmemset_source
OBJS += pmemset_source.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug
LIBPMEMCOMMON=y
include ../Makefile.inc
