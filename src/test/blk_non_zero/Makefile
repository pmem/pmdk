# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/blk_non_zero/Makefile -- build blk_non_zero unit test
#
TARGET = blk_non_zero
OBJS = blk_non_zero.o

LIBPMEMBLK=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

INCS += -I../../libpmemblk
