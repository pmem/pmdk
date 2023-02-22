# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# src/test/log_include/Makefile -- compilation test for libpmemlog
#
TARGET = log_include
OBJS = log_include.o

LIBPMEMLOG=y

include ../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations
