# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2023, Intel Corporation

#
# src/test/compat_incompat_features/Makefile -- build test for compat/incompat
#                                               feature flags
#
TARGET = pool_open
OBJS = pool_open.o

LIBPMEMOBJ=y
LIBPMEMBLK=y
LIBPMEMLOG=y
USE_PMEMSPOIL=y

include ../Makefile.inc

# Libpmemblk and libpmemlog are deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk/pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
