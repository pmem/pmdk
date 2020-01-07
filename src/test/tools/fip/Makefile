# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation
#
# Makefile -- Makefile for fip tool
#

TOP = ../../../..
include $(TOP)/src/common.inc

vpath %.c $(TOP)/src/rpmem_common/
vpath %.c $(TOP)/src/core/

ifeq ($(BUILD_RPMEM),y)
SCP_TO_REMOTE_NODES = y
TARGET = fip

OBJS = fip.o rpmem_fip_common.o rpmem_common.o util.o os_posix.o alloc.o

else
$(info NOTE: Skipping fip because $(BUILD_RPMEM_INFO))
endif

include $(TOP)/src/tools/Makefile.inc

ifeq ($(BUILD_RPMEM),y)
CFLAGS += -I$(TOP)/src/rpmem_common
CFLAGS += -I$(TOP)/src/core
CFLAGS += $(shell $(PKG_CONFIG) --cflags libfabric)
LIBS += $(shell $(PKG_CONFIG) --libs libfabric)
endif
