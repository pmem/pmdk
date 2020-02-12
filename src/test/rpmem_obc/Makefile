# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/rpmem_obc/Makefile -- build rpmem_obc test
#
SCP_TO_REMOTE_NODES = y

vpath %.c ../../librpmem/
vpath %.c ../../rpmem_common/

TARGET = rpmem_obc
OBJS = rpmem_obc_test.o\
       rpmem_obc_test_common.o\
       rpmem_obc_test_misc.o\
       rpmem_obc_test_create.o\
       rpmem_obc_test_open.o\
       rpmem_obc_test_set_attr.o\
       rpmem_obc_test_close.o\
       rpmem_obc.o rpmem_util.o rpmem_common.o\
       rpmem_ssh.o rpmem_cmd.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCOMMON=y
include ../Makefile.inc

CFLAGS += -DRPMEMC_LOG_RPMEM -DDEBUG
INCS += -I../../librpmem/
INCS += -I../../rpmem_common/
