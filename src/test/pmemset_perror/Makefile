# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

#
# src/test/pmemset_perror/Makefile -- build pmemset_perror unit test
#
TOP = ../../..

vpath %.c $(TOP)/src/test/unittest
vpath %.c $(TOP)/src/libpmemset

INCS += -I$(TOP)/src/libpmemset
TARGET = pmemset_perror
OBJS += config.o\
	errormsg.o\
	pmemset_perror.o\
	pmemset_utils.o

LIBPMEMCORE=y

include ../Makefile.inc
