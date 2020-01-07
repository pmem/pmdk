# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/rpmemd_db_test/Makefile -- build rpmemd_db_test unit test
#

TOP = ../../..

vpath %.c $(TOP)/src/rpmem_common
vpath %.c $(TOP)/src/librpmem
vpath %.c $(TOP)/src/tools/rpmemd

TARGET = rpmemd_db
OBJS = rpmemd_db_test.o rpmemd_log.o rpmemd_db.o

LIBPMEMCOMMON=y

include ../Makefile.inc

INCS += -I$(TOP)/src/rpmem_common
INCS += -I$(TOP)/src/librpmem
INCS += -I$(TOP)/src/tools/rpmemd
