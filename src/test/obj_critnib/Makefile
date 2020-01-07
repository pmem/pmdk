# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_critnib/Makefile -- build obj_critnib unit test
#
TARGET = obj_critnib
OBJS = obj_critnib.o

LIBPMEMOBJ=internal-debug

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_critnib.c)
