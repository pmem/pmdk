# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_persist_count/Makefile -- build obj_persist_count unit test
#
TARGET = obj_persist_count
OBJS = obj_persist_count.o

LIBPMEMOBJ=internal-debug

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_persist_count.c)

