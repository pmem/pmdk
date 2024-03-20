# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = pmem_log_set_function
OBJS = pmem_log_set_function.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# required for proper mock integration
LIBPMEMCOMMON=internal-debug
LIBPMEM=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, pmem_log_set_function.c)
