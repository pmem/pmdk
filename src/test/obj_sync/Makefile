# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_sync/Makefile -- build obj_sync unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/libpmemobj

TARGET = obj_sync
OBJS = obj_sync.o sync.o mocks_posix.o

LIBPMEMCOMMON=y

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, mocks_posix.c)

INCS += -I$(TOP)/src/libpmemobj/
