# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

#
# src/test/ctl_cow/Makefile -- build ctl_cow test
#
TARGET = ctl_cow
OBJS = ctl_cow.o

LIBPMEMBLK=y
LIBPMEMOBJ=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
