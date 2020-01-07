# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2018, Intel Corporation

#
# src/test/rpmem_addr_ext/Makefile -- build rpmem_addr_ext test
#

include ../../common.inc

vpath %.c ../../rpmem_common

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
TARGET = rpmem_addr_ext
OBJS = rpmem_addr_ext.o

LIBRPMEM=y
endif

include ../Makefile.inc

ifeq ($(BUILD_RPMEM), y)
LIBS += $(shell $(PKG_CONFIG) --libs libfabric)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libfabric)
INCS += -I../../rpmem_common
endif
