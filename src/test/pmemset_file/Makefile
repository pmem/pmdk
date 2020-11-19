# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmemset_file/Makefile -- build for pmemset file API unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_file
OBJS += pmemset_file.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug
LIBPMEMCOMMON=y
include ../Makefile.inc
