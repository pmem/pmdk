# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/rpmemd_obc/Makefile -- build rpmemd_obc test
#
SCP_TO_REMOTE_NODES = y

vpath %.c ../../tools/rpmemd
vpath %.c ../../rpmem_common
vpath %.c ../../librpmem

TARGET = rpmemd_obc
OBJS = rpmemd_obc_test.o\
       rpmemd_obc_test_common.o\
       rpmemd_log.o\
       rpmem_common.o\
       rpmem_util.o\
       rpmem_ssh.o\
       rpmem_cmd.o\
       rpmemd_obc.o\
       rpmemd_obc_test_msg_hdr.o\
       rpmemd_obc_test_misc.o\
       rpmemd_obc_test_create.o\
       rpmemd_obc_test_open.o\
       rpmemd_obc_test_set_attr.o\
       rpmemd_obc_test_close.o

BIULD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCOMMON=y
include ../Makefile.inc

CFLAGS += -DRPMEMC_LOG_RPMEMD
INCS += -I../../tools/rpmemd
INCS += -I../../rpmem_common
INCS += -I../../librpmem
