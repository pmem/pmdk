# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

#
# src/test/rpmem_fip/Makefile -- build rpmem_fip test
#

include ../../common.inc

vpath %.c ../../librpmem/
vpath %.c ../../rpmem_common/
vpath %.c ../../tools/rpmemd

ifeq ($(BUILD_RPMEM), y)
SCP_TO_REMOTE_NODES = y
TARGET = rpmem_fip
OBJS = rpmem_fip_test.o\
       rpmem_fip_oob.o\
       rpmem_fip.o\
       rpmemd_fip.o\
       rpmem_util.o\
       rpmem_ssh.o\
       rpmem_cmd.o\
       rpmem_common.o\
       rpmem_fip_common.o\
       rpmemd_util.o\
       rpmemd_obc.o\
       rpmemd_log.o

endif

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCOMMON=y

include ../Makefile.inc

ifeq ($(BUILD_RPMEM), y)
LIBS += $(LIBFABRIC_LIBS)
CFLAGS += $(LIBFABRIC_CFLAGS)
CFLAGS += -DDEBUG
CFLAGS += -DRPMEMC_LOG_RPMEM
INCS += -I../../librpmem/
INCS += -I../../rpmem_common/
INCS += -I../../tools/rpmemd
endif
