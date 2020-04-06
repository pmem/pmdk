# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation

#
# src/test/tools/obj_verify -- build obj_verify tool
#

SCP_TO_REMOTE_NODES = y

TOP = ../../../..

TARGET = obj_verify
OBJS = obj_verify.o

LIBPMEM=y
LIBPMEMOBJ=y
LIBPMEMCOMMON=y

include $(TOP)/src/tools/Makefile.inc

USE_LIBUNWIND := $(shell $(PKG_CONFIG) libunwind && echo y || echo n)

ifeq ($(USE_LIBUNWIND),y)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libunwind) -DUSE_LIBUNWIND
LIBUNWIND_LIBS := $(shell $(PKG_CONFIG) --libs libunwind)
UT = ../../unittest/libut.a
LIBS += $(UT) $(LIBUNWIND_LIBS) $(LIBDL)
endif
