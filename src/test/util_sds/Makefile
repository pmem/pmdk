# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2018, Intel Corporation

#
# src/test/util_sds/Makefile -- shutdown_tests unit test
#
TARGET = util_sds
OBJS = util_sds.o

LIBPMEMCOMMON=internal-debug
LIBPMEM=y

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, util_sds.c)
