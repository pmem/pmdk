# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/pmem2_config_get_alignment/Makefile -- build
# pmem2_config_get_alignment unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmem2

INCS += -I$(TOP)/src/libpmem2
TARGET = pmem2_config_get_alignment
OBJS += errormsg.o\
	pmem2_config_get_alignment.o\
	ut_pmem2_config.o\
	ut_pmem2_utils.o

LIBPMEMCOMMON=y
include ../Makefile.inc
