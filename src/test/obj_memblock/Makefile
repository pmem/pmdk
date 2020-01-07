# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_memblock/Makefile -- build memblock unit test
#
TARGET = obj_memblock
OBJS = obj_memblock.o

LIBPMEMOBJ=internal-debug

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_memblock.c)
