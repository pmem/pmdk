# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2024, Intel Corporation
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
	extent_linux.o\
	pmem2_utils_linux.o\
	pmem2_utils.o\
	region_namespace_$(OS_DIMM).o

LIBPMEMCORE=y

include $(TOP)/src/tools/Makefile.inc
