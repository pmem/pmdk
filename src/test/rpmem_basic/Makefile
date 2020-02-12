# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/rpmem_basic/Makefile -- build rpmem_basic test
#

include ../../common.inc

vpath %.c ../../rpmem_common

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
TARGET = rpmem_basic
OBJS = rpmem_basic.o\
       rpmem_fip_common.o\
       rpmem_common.o

LIBRPMEM=y
LIBPMEMCOMMON=y
endif

include ../Makefile.inc

ifeq ($(BUILD_RPMEM), y)
LIBS += $(shell $(PKG_CONFIG) --libs libfabric)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libfabric)
INCS += -I../../rpmem_common
endif
