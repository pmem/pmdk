# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = core_log_last
OBJS = core_log_last.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCORE=internal-debug
EXTRA_CFLAGS=-Dnoreturn="/* noreturn */"

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, core_log_last.c)
