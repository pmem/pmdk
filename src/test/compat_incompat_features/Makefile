# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2023, Intel Corporation

#
# src/test/compat_incompat_features/Makefile -- build test for compat/incompat
#                                               feature flags
#
TARGET = pool_open
OBJS = pool_open.o

LIBPMEMOBJ=y
USE_PMEMSPOIL=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
