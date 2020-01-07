# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#
# Makefile -- Makefile for gran_detecto tool
#

TOP = ../../../..

vpath %.c $(TOP)/src/core
INCS += -I$(TOP)/src/core
INCS += -I$(TOP)/src/common

TARGET = gran_detecto

OBJS += gran_detecto.o\
	os_posix.o

LIBPMEM2=y

include $(TOP)/src/tools/Makefile.inc
