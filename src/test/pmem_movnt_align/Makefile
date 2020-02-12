# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# src/test/pmem_movnt_align/Makefile -- build pmem_movnt_align unittest
#
TOP = ../../..
vpath %.c $(TOP)/src/test/pmem2_movnt_align

TARGET = pmem_movnt_align
OBJS = pmem_movnt_align.o\
	movnt_align_common.o

LIBPMEM=y

include ../Makefile.inc

CFLAGS += -I$(TOP)/src/test/pmem2_movnt_align
