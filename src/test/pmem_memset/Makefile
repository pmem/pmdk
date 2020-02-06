# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# src/test/pmem_memset/Makefile -- build pmem_memset unit test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/pmem2_memset

TARGET = pmem_memset
OBJS = pmem_memset.o\
	memset_common.o

LIBPMEM=y
LIBPMEMCOMMON=y

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_memset
