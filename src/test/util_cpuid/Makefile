# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/util_cpuid/Makefile -- build util_cpu unit test
#
vpath %.c ../../libpmem2/x86_64

TARGET = util_cpuid
OBJS = util_cpuid.o cpu.o

LIBPMEMCOMMON=y
include ../Makefile.inc

INCS += -I../../libpmem2/x86_64
