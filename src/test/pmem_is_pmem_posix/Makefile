# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2022, Intel Corporation

#
# src/test/pmem_is_pmem_posix/Makefile -- build pmem_is_pmem_posix unit test
#
include ../../common.inc
TOP = ../../..
vpath %.c $(TOP)/src/libpmem
vpath %.c $(TOP)/src/libpmem2
vpath %.c $(TOP)/src/libpmem2/$(ARCH)

TARGET = pmem_is_pmem_posix
OBJS = pmem_is_pmem_posix.o\
	libpmem.o\
	pmem.o\
	pmem_posix.o\
	memops_generic.o

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
ifeq ($(ARCH), loongarch64)
OBJS += init.o
endif

LIBPMEMCOMMON=y

include ../Makefile.inc

CFLAGS += -DSRCVERSION="" -DDEBUG -DMOVDIR64B_AVAILABLE=0 -DAVX512F_AVAILABLE=0 -DAVX_AVAILABLE=0 -DSSE2_AVAILABLE=0
CFLAGS += -I$(TOP)/src/libpmem
CFLAGS += -I$(TOP)/src/libpmem/$(ARCH)
CFLAGS += -I$(TOP)/src/libpmem2
