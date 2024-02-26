# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = core_log_internal
OBJS = core_log_internal.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# 'internal' is required for proper mock integration
# 'nondebug' is required for proper execution of test_CORE_LOG_TRESHOLD_DEFAULT
# (use nondebug version of core/log.o)
LIBPMEMCORE=internal-nondebug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, core_log_internal.c)
