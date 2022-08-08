# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Intel Corporation
#
# Makefile -- Makefile for bttcreate tool
#
TOP = ../../../..

vpath %.c $(TOP)/src/libpmemblk/

TARGET = bttcreate
OBJS = bttcreate.o btt.o

LIBPMEMCOMMON=y
LIBPMEM=y

include $(TOP)/src/tools/Makefile.inc
INCS += -I$(TOP)/src/libpmemblk
