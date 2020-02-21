# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# src/test/util_sds/Makefile -- shutdown_tests unit test
#
TOP = ../../..
vpath %.c $(TOP)/src/test/unittest/
vpath %.c $(TOP)/src/common/

TARGET = util_sds
OBJS = util_sds.o\
	shutdown_state.o\
	ut_pmem2_config.o\
	ut_pmem2_utils.o

LIBPMEM2=internal-debug

include ../Makefile.inc
# add DEBUG define for logs from shutdown_state.c
CFLAGS += -DDEBUG=1
LDFLAGS += $(call extract_funcs, util_sds.c)
