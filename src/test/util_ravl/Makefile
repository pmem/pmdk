# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2020, Intel Corporation
#

#
# src/test/util_ravl/Makefile -- build ravl tree unit test
#

TARGET = util_ravl
OBJS = util_ravl.o alloc.o ravl.o out.o util_posix.o util.o
INCS += -I$(TOP)/src/common

include ../Makefile.inc
vpath %.c $(TOP)/src/core
vpath %.c $(TOP)/src/common
CFLAGS += -DSRCVERSION=""
