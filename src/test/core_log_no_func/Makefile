# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = core_log_no_func
OBJS = core_log_no_func.o ../core_log/core_log_common.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# `internal` is required for proper mock integration
LIBPMEMCORE=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, ../core_log/core_log_common.c)
INCS += -I../core_log
