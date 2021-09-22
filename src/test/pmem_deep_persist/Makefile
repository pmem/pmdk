# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2021, Intel Corporation

#
# src/test/pmem_deep_persist/Makefile -- build pmem_deep_persist test
#
include ../../common.inc

TOP = ../../..
vpath %.c $(TOP)/src/libpmem
vpath %.c $(TOP)/src/libpmem2
vpath %.c $(TOP)/src/libpmem2/$(ARCH)
vpath %.c $(TOP)/src/libpmemobj

TARGET = pmem_deep_persist

OBJS = pmem_deep_persist.o\
	pmem.o\
	pmem_posix.o\
	memops_generic.o\
	mocks_posix.o

ifeq ($(ARCH), aarch64)
OBJS += init.o
endif
ifeq ($(ARCH), riscv64)
OBJS += init.o
endif
ifeq ($(ARCH), x86_64)
OBJS += init.o cpu.o
endif
ifeq ($(ARCH), ppc64)
include $(TOP)/src/libpmem2/$(ARCH)/sources.inc
OBJS += $(LIBPMEM2_ARCH_SOURCE:%.c=%.o)
endif

LIBPMEMOBJ=internal-debug

include ../Makefile.inc

CFLAGS += -DSRCVERSION="" -DDEBUG -DAVX512F_AVAILABLE=0 -DAVX_AVAILABLE=0 -DSSE2_AVAILABLE=0
CFLAGS += -I$(TOP)/src/libpmem
CFLAGS += -I$(TOP)/src/libpmem2
CFLAGS += -I$(TOP)/src/libpmemobj/
LDFLAGS += $(call extract_funcs, mocks_posix.c)
