# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/log_pool_lock/Makefile -- build log_pool_lock unit test
#
TARGET = log_pool_lock
OBJS = log_pool_lock.o

LIBPMEMLOG=y

include ../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

