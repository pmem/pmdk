# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# src/test/pmem_memmove/Makefile -- build pmem_memmove unit test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/pmem2_memmove

TARGET = pmem_memmove
OBJS = pmem_memmove.o\
	memmove_common.o

LIBPMEM=y
LIBPMEMCOMMON=y

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_memmove
