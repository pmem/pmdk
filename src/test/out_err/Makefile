# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2016, Intel Corporation

#
# src/test/out_err/Makefile -- build unit test for out_err()
#
TARGET = out_err
OBJS = out_err.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCOMMON=y
include ../Makefile.inc

CFLAGS += -DDEBUG
