# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

#
# src/test/rpmemd_dbg/Makefile -- build rpmemd_dbg test
#
TOP = ../../..
vpath %.c $(TOP)/src/tools/rpmemd
vpath %.c ../rpmemd_log/

TARGET = rpmemd_dbg
OBJS = rpmemd_log_test.o rpmemd_log.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n
LIBPMEMCOMMON=y

include ../Makefile.inc

CFLAGS += -I../../tools/rpmemd -DDEBUG
LDFLAGS += $(call extract_funcs, ../rpmemd_log/rpmemd_log_test.c)
