# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
# Makefile -- Makefile for mapexec tool
#

TOP = ../../../..

vpath %.c $(TOP)/src/core
INCS += -I$(TOP)/src/core

TARGET = mapexec

OBJS += mapexec.o\
	os_posix.o

include $(TOP)/src/tools/Makefile.inc
