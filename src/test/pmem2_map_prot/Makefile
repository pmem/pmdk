# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmem2_map_prot/Makefile -- build pmem2_map_prot unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_map_prot
OBJS += pmem2_map_prot.o\
	ut_pmem2_config.o\
	ut_pmem2_utils.o\
	ut_pmem2_source.o

LIBPMEM2=internal-debug

include ../Makefile.inc
