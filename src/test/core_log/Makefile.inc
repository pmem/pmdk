# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# required for proper mock integration
LIBPMEMCORE=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, ../core_log/core_log_common.c)
