# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2023, Intel Corporation

#
# src/test/ctl_prefault/Makefile -- build ctl_prefault test
#
TARGET = ctl_prefault
OBJS = ctl_prefault.o

LIBPMEMBLK=y
LIBPMEMLOG=y
LIBPMEMOBJ=y

include ../Makefile.inc

# Libpmemblk is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
