# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

#
# src/test/pmem2_persist/Makefile -- build pmem2_persist unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/libpmem2
vpath %.c $(TOP)/src/core

INCS += -I$(TOP)/src/libpmem2

TARGET = pmem2_persist

LIBPMEMCORE=internal-debug
OBJS += pmem2_persist.o\
	persist.o\
	memops_generic.o\
	deep_flush_linux.o\
	pmem2_utils_linux.o

ifeq ($(OS_DIMM),ndctl)
LINK_NDCTL=y
OBJS += region_namespace_ndctl.o
else
OBJS += region_namespace_none.o
endif

include ../Makefile.inc
