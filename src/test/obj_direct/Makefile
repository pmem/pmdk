# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/obj_direct/Makefile -- build obj_direct unit test
#
vpath %.c ../../libpmemobj

TARGET = obj_direct
OBJS = obj_direct.o obj_direct_inline.o obj_direct_non_inline.o

LIBPMEMOBJ=y

include ../Makefile.inc

INCS += -I../../libpmemobj/
