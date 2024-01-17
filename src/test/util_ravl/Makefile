# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2024, Intel Corporation
#

#
# src/test/util_ravl/Makefile -- build ravl tree unit test
#

TARGET = util_ravl
OBJS = util_ravl.o
LIBPMEMCORE = y
INCS += -I$(TOP)/src/common

include ../Makefile.inc
vpath %.c $(TOP)/src/core
vpath %.c $(TOP)/src/common
CFLAGS += -DSRCVERSION=""
