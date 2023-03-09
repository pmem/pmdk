# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/set_funcs/Makefile -- build set_funcs unit test
#
TARGET = set_funcs
OBJS = set_funcs.o

LIBPMEM=y
LIBPMEMOBJ=y
LIBPMEMBLK=y
LIBPMEMLOG=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
