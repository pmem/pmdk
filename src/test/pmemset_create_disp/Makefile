# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

#
# src/test/pmemset_file/Makefile -- build for pmemset file API unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_create_disp
OBJS += pmemset_create_disp.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc
