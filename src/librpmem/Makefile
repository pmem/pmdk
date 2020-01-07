# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/librpmem/Makefile -- Makefile for librpmem
#
include ../common.inc

vpath %.c ../rpmem_common

ifeq ($(BUILD_RPMEM),y)
LIBRARY_NAME = rpmem
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE = $(COMMON)/alloc.c\
	$(COMMON)/os_posix.c\
	$(COMMON)/os_thread_posix.c\
	$(COMMON)/out.c\
	$(COMMON)/util.c\
	$(COMMON)/util_posix.c\
	librpmem.c\
	rpmem.c\
	rpmem_obc.c\
	rpmem_cmd.c\
	rpmem_ssh.c\
	rpmem_common.c\
	rpmem_util.c\
	rpmem_fip_common.c\
	rpmem_fip.c

else
$(info NOTE: Skipping librpmem because $(BUILD_RPMEM_INFO))
endif

include ../Makefile.inc

ifeq ($(BUILD_RPMEM),y)
LIBS += -pthread
LIBS += $(shell $(PKG_CONFIG) --libs libfabric)
CFLAGS += $(shell $(PKG_CONFIG) --cflags libfabric)
CFLAGS += -I. -I../rpmem_common
CFLAGS += -DRPMEMC_LOG_RPMEM
endif
