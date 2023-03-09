# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/blk_nblock/Makefile -- build blk_nblock unit test
#
TARGET = blk_nblock
OBJS = blk_nblock.o

LIBPMEMBLK=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
