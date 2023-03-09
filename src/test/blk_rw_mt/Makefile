# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/blk_rw_mt/Makefile -- build blk_rw_mt unit test
#
TARGET = blk_rw_mt
OBJS = blk_rw_mt.o

LIBPMEMBLK=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
