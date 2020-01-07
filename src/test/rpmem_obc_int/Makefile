# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Intel Corporation

#
# src/test/rpmem_obc_int/Makefile -- build rpmem_obc_int test
#
SCP_TO_REMOTE_NODES = y

vpath %.c ../../librpmem/
vpath %.c ../../rpmem_common/
vpath %.c ../../tools/rpmemd

TARGET = rpmem_obc_int
OBJS = rpmem_obc_int.o\
       rpmem_cmd.o rpmem_ssh.o\
       rpmem_obc.o rpmem_util.o rpmem_common.o\
       rpmemd_obc.o rpmemd_log.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

LIBPMEMCOMMON=y
include ../Makefile.inc

INCS += -I../../librpmem/
INCS += -I../../rpmem_common/
INCS += -I../../tools/rpmemd
