# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = obj_log_set_function
OBJS = obj_log_set_function.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# required for proper mock integration
LIBPMEMOBJ=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, obj_log_set_function.c)
