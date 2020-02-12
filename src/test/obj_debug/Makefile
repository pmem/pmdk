# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_debug/Makefile -- build obj_debug unit test
#
TARGET = obj_debug
OBJS = obj_debug.o

LIBPMEMOBJ=y

BUILD_STATIC_NONDEBUG=n

include ../Makefile.inc

CFLAGS += -DDEBUG
