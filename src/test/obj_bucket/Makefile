# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_bucket/Makefile -- build bucket unit test
#
TARGET = obj_bucket
OBJS = obj_bucket.o

LIBPMEMOBJ=internal-debug

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_bucket.c)
