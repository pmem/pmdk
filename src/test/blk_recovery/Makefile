# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/blk_recovery/Makefile -- build blk_recovery unit test
#
TARGET = blk_recovery
OBJS = blk_recovery.o

LIBPMEMBLK=y

include ../Makefile.inc
CFLAGS += -I../../libpmemblk

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
