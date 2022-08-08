# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2017, Intel Corporation
#
# Makefile -- Makefile for pmemalloc tool
#

TOP = ../../../..

TARGET = pmemalloc

OBJS = pmemalloc.o

LIBPMEM=y
LIBPMEMCOMMON=y
LIBPMEMOBJ=y

CFLAGS += -I$(TOP)/src/libpmemobj

include $(TOP)/src/tools/Makefile.inc
