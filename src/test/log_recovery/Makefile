# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/test/log_recovery/Makefile -- build log_recovery unit test
#
TARGET = log_recovery
OBJS = log_recovery.o

LIBPMEMLOG=y

include ../Makefile.inc
CFLAGS += -I../../libpmemlog

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

