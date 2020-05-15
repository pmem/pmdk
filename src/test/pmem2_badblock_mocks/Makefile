# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/test/pmem2_badblock_mocks/Makefile --
#           -- build pmem2_badblock_mocks test
#

TOP = ../../..

include ../Makefile.inc

ifeq ($(NDCTL_ENABLE),y)

TARGET = pmem2_badblock_mocks

OBJS += pmem2_badblock_mocks.o\
	mocks_ndctl.o\
	mocks_pmem2.o\
	mocks_other.o

LIBPMEM2=internal-debug

LDFLAGS += $(call extract_funcs, mocks_ndctl.c)
LDFLAGS += $(call extract_funcs, mocks_pmem2.c)
LDFLAGS += $(call extract_funcs, mocks_other.c)

else

$(info NOTE: Skipping pmem2_badblock_mocks test because ndctl is not available)

endif
