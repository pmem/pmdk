# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation
#
# Makefile -- top Makefile for rpmemd
#

vpath %.c ../../rpmem_common/

TOP = ../../..
include $(TOP)/src/common.inc

ifeq ($(BUILD_RPMEM),y)
SCP_TO_REMOTE_NODES = y
TARGET = rpmemd
OBJS = rpmemd.o\
       rpmemd_log.o\
       rpmemd_config.o\
       rpmem_common.o\
       rpmemd_obc.o\
       rpmemd_db.o\
       rpmemd_fip.o\
       rpmem_fip_common.o\
       rpmemd_util.o

LIBPMEM=y
TOOLS_COMMON=y
LIBPMEMBLK_PRIV=btt_info_convert2h

INCS += -I$(TOP)/src/rpmem_common
CFLAGS += -DRPMEMC_LOG_RPMEMD
ifneq ($(DEBUG),)
CFLAGS += -DDEBUG
endif
CFLAGS += $(LIBFABRIC_CFLAGS)

LIBS += -pthread
LIBS += $(LIBFABRIC_LIBS)

INSTALL_TARGET=y

else
$(info NOTE: Skipping rpmemd because $(BUILD_RPMEM_INFO))
endif

include ../Makefile.inc

.PHONY: test check
