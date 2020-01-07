# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_list/Makefile -- build obj_list test
#
TARGET = obj_list
OBJS = obj_list.o obj_list_mocks.o obj_list_mocks_palloc.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMOBJ=internal-debug

include ../Makefile.inc
CFLAGS += -g -DDEBUG
LDFLAGS += $(call extract_funcs, obj_list.c)
LDFLAGS += $(call extract_funcs, obj_list_mocks.c)
LDFLAGS += $(call extract_funcs, obj_list_mocks_palloc.c)

