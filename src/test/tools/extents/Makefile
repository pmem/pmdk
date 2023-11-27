# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2023, Intel Corporation
#
# Makefile -- Makefile for extents tool
#

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

OBJS += extent_linux.o

LIBPMEMCORE=y

include $(TOP)/src/tools/Makefile.inc
