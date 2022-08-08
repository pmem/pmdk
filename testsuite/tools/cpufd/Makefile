# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
# Makefile -- Makefile for cpufd tool
#

TOP = ../../../..

INCS += -I$(TOP)/src/core
vpath %.c $(TOP)/src/libpmem2/x86_64
INCS += -I$(TOP)/src/libpmem2/x86_64

TARGET = cpufd

OBJS += cpufd.o\
	cpu.o

include $(TOP)/src/tools/Makefile.inc
