# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# src/test/blk_include/Makefile -- compilation test for libpmemblk
#
TARGET = blk_include
OBJS = blk_include.o

LIBPMEMBLK=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
