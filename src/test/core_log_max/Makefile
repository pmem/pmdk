# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = core_log_max
OBJS = core_log_max.o call_all.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCORE=y
include ../Makefile.inc
LDFLAGS += $(call extract_funcs, core_log_max.c)
