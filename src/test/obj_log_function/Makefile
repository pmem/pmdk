# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = obj_log_function
OBJS = obj_log_function.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# 'internal' is required for proper mock integration
# 'debug' is required for debug version of core/log.o that provides
# implementation of 'out_log()' that is used by 'ut_log_function()'
LIBPMEMOBJ=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, obj_log_function.c)
