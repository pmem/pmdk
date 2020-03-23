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
	$(call osdep, extent,.o)\
	pmem2_utils_linux.o\
	pmem2_utils.o

LIBPMEMCORE=y

include $(TOP)/src/tools/Makefile.inc
