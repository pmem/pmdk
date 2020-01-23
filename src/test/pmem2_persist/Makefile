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

OBJS += alloc.o\
	pmem2_persist.o\
	persist.o\
	util.o\
	memops_generic.o

include ../Makefile.inc
