# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2023, Intel Corporation

#
# src/test/log_pool/Makefile -- build log_pool unit test
#
TARGET = log_pool
OBJS = log_pool.o

LIBPMEMLOG=y
USE_PMEMSPOIL=y

include ../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
