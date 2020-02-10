# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

#
# src/test/pmem2_integration/Makefile -- build pmem2_integration test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_integration
OBJS = pmem2_integration.o\
	ut_pmem2_utils.o\
	ut_pmem2_config.o

LIBPMEM2=y

include ../Makefile.inc
