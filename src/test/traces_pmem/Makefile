# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/traces_pmem/Makefile -- build traces for library pmem
#
TARGET = traces_pmem
OBJS = traces_pmem.o

LIBPMEM=y
LIBPMEMBLK=y
LIBPMEMLOG=y
LIBPMEMOBJ=y

include ../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
