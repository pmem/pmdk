# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

#
# src/test/rpmemd_log/Makefile -- build rpmemd_log test
#
TOP = ../../..
vpath %.c $(TOP)/src/tools/rpmemd

TARGET = rpmemd_log
OBJS = rpmemd_log_test.o rpmemd_log.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n
LIBPMEMCOMMON=y

include ../Makefile.inc

CFLAGS += -I../../tools/rpmemd
LDFLAGS += $(call extract_funcs, rpmemd_log_test.c)
