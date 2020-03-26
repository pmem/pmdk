# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

#
# src/test/pmem2_integration/Makefile -- build pmem2_integration test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_api
OBJS = pmem2_api.o\
	ut_pmem2_config.o\
	ut_pmem2_map.o\
	ut_pmem2_source.o\
	ut_pmem2_utils.o

LIBPMEM2=y

include ../Makefile.inc
