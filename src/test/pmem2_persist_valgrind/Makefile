# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2024, Intel Corporation

#
# src/test/pmem2_persist_valgrind/Makefile -- build pmem2_persist_valgrind test
#
TOP = ../../..

vpath %.c $(TOP)/src/core
vpath %.c $(TOP)/src/test/unittest

TARGET = pmem2_persist_valgrind
OBJS = pmem2_persist_valgrind.o\
	ut_pmem2_utils.o

LIBPMEM2=y

include ../Makefile.inc
CFLAGS += -DSRCVERSION='"$(SRCVERSION)"'
