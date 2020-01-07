# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/util_poolset/Makefile -- build util_poolset unit test
#
TARGET = util_poolset
OBJS = util_poolset.o mocks_posix.o

LIBPMEMCOMMON=y

USE_PMEMSPOIL=y

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, mocks_posix.c)
