# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_critnib_mt/Makefile -- build obj_critnib_mt unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/libpmemobj

TARGET = obj_critnib_mt
OBJS = obj_critnib_mt.o critnib.o

LIBPMEMCOMMON=y

include ../Makefile.inc

LDFLAGS += $(call extract_funcs, obj_critnib_mt.c)

INCS += -I$(TOP)/src/libpmemobj/
