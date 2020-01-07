# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2018, Intel Corporation

#
# src/test/obj_sds/Makefile -- shutdown_state unit test
#
TARGET = obj_sds
OBJS = obj_sds.o

LIBPMEMPOOL=y
LIBPMEMOBJ=internal-debug
include ../Makefile.inc
LDFLAGS += $(call extract_funcs, obj_sds.c)
