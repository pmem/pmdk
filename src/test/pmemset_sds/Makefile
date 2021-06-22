# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

#
# src/test/pmemset_sds/Makefile -- build for pmemset SDS API unit tests
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmemset

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_sds
OBJS += pmemset_sds.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc
