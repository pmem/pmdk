# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation

#
# src/test/rpmemd_util/Makefile -- build rpmemd_util test
#
TOP = ../../..
vpath %.c $(TOP)/src/tools/rpmemd
vpath %.c $(TOP)/src/rpmem_common

TARGET = rpmemd_util

OBJS = rpmemd_util_test.o rpmem_common.o rpmemd_log.o rpmemd_util.o

LIBPMEM=y

include ../Makefile.inc

INCS += -I$(TOP)/src/rpmem_common
CFLAGS += -I../../tools/rpmemd
