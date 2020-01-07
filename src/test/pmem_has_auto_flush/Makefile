# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation

#
# src/test/pmem_has_auto_flush/Makefile -- build pmem_has_auto_flush test
#
include ../../common.inc

LIBPMEMCOMMON=internal-debug
LIBPMEM=internal-debug

TARGET = pmem_has_auto_flush

OBJS = pmem_has_auto_flush.o \
	mocks_posix.o

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, mocks_posix.c)
