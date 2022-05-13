# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#

#
# src/test/pmem2_future/Makefile -- build pmem2_future unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_future
OBJS = pmem2_future.o\
	ut_pmem2_utils.o\
	ut_pmem2_config.o\
	ut_pmem2_source.o\
	ut_pmem2_setup_integration.o

LIBPMEM2=y
LIBMINIASYNC=y
include ../Makefile.inc
