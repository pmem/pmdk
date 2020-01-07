# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

#
# src/test/rpmem_addr/Makefile -- build rpmem_addr test
#
include ../../common.inc

vpath %.c ../../rpmem_common

TARGET = rpmem_addr
OBJS = rpmem_addr.o\
       rpmem_common.o

include ../Makefile.inc

CFLAGS += -I../../rpmem_common
