# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/blk_pool_lock/Makefile -- build blk_pool_lock unit test
#
TARGET = blk_pool_lock
OBJS = blk_pool_lock.o

LIBPMEMBLK=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
