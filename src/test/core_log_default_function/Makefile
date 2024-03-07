# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = core_log_default_function
OBJS = core_log_default_function.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# required for proper mock integration
LIBPMEMCORE=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, core_log_default_function.c)
