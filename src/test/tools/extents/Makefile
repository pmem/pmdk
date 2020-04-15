# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation
#
# Makefile -- Makefile for extents tool
#

SCP_TO_REMOTE_NODES = y

TOP = ../../../..

INCS += -I$(TOP)/src/core
INCS += -I$(TOP)/src/common
INCS += -I$(TOP)/src/libpmem2

vpath %.c $(TOP)/src/libpmem2

TARGET = extents
OBJS = extents.o\
	pmem2_utils_linux.o\
	pmem2_utils.o

ifeq ($(OS_DIMM),ndctl)
OBJS += region_namespace_ndctl.o
else
OBJS += region_namespace_none.o
endif

ifeq ($(OS_KERNEL_NAME),Linux)
OBJS += extent_linux.o
else
OBJS += extent_none.o
endif

LIBPMEMCORE=y

include $(TOP)/src/tools/Makefile.inc
