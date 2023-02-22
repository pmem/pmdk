# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/log_walker/Makefile -- build log_walker unit test
#
TARGET = log_walker
OBJS = log_walker.o

LIBPMEMLOG=y

include ../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
