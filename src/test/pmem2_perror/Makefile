# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmem2_perror/Makefile -- build pmem2_perror unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmem2

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_perror
OBJS += errormsg.o\
	pmem2_perror.o

LIBPMEMCOMMON=y

include ../Makefile.inc
