# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_heap_interrupt/Makefile -- build obj_heap_interrupt unit test
#
TARGET = obj_heap_interrupt
OBJS = obj_heap_interrupt.o

LIBPMEMOBJ=internal-debug

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_heap_interrupt.c)
