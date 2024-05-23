# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

#
# src/test/obj_ulog_advanced/Makefile -- build obj_ulog_advanced test
#
TARGET = obj_ulog_advanced
OBJS = obj_ulog_advanced.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# required for proper mock integration
LIBPMEMOBJ=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, obj_ulog_advanced.c)
