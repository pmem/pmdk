# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation
#
# Makefile -- Makefile for ctrld tool
#

SCP_TO_REMOTE_NODES = y

TOP = ../../../..

vpath %.c $(TOP)/src/core

CFLAGS += -I$(TOP)/src/core
CFLAGS += -I$(TOP)/src/common

TARGET = ctrld

OBJS = ctrld.o os_posix.o

include $(TOP)/src/tools/Makefile.inc
