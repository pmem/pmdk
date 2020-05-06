# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

#
# src/test/pmem2_map/Makefile -- build pmem2_map unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_map
OBJS += pmem2_map.o\
	ut_pmem2_config.o\
	ut_pmem2_utils.o\
	ut_pmem2_source.o\
	ut_setup.o

LIBPMEM2=internal-debug

include ../Makefile.inc
