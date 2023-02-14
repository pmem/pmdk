# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/blk_pool/Makefile -- build blk_pool unit test
#
TARGET = blk_pool
OBJS = blk_pool.o

LIBPMEMBLK=y
USE_PMEMSPOIL=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
