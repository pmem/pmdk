# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# src/test/pmem2_mover/Makefile -- build pmem2_mover test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_mover
OBJS = pmem2_mover.o\
	ut_pmem2_utils.o\
	ut_pmem2_config.o\
	ut_pmem2_source.o\
	ut_pmem2_setup_integration.o

LIBPMEM2=y
LINMINIASYNC=y
include ../Makefile.inc
