# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

TARGET = core_log_threshold_mt
OBJS = core_log_threshold_mt.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCORE=nondebug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, core_log_threshold_mt.c)
