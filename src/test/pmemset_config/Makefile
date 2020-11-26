# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmemset_config/Makefile -- build pmemset_config unit test
#

TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmemset_config
OBJS += pmemset_config.o\
	ut_pmemset_utils.o

LIBPMEMSET=internal-debug

include ../Makefile.inc
