# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# src/test/pmem_memcpy/Makefile -- build pmem_memcpy unit test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/pmem2_memcpy

TARGET = pmem_memcpy
OBJS = pmem_memcpy.o\
	memcpy_common.o

LIBPMEM=y
LIBPMEMCOMMON=y

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_memcpy
