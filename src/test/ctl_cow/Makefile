# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

#
# src/test/ctl_cow/Makefile -- build ctl_cow test
#
TARGET = ctl_cow
OBJS = ctl_cow.o

LIBPMEMBLK=y
LIBPMEMLOG=y
LIBPMEMOBJ=y

include ../Makefile.inc

# Libpmemblk and libpmemlog are deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk/pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
