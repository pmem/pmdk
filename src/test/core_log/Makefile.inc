# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# 'internal' is required for proper mock integration
# 'debug' provides 'out_log()' required for 'ut_log_function()'
LIBPMEMCORE=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, ../core_log/core_log_common.c)
