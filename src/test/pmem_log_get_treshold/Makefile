# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#

#
# src/test/pmem_log_get_treshold/Makefile -- build pmem_log_get_treshold unit test
#

TARGET = pmem_log_get_treshold
OBJS = pmem_log_get_treshold.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n

# required for proper mock integration
LIBPMEMCOMMON=internal-debug
LIBPMEM=internal-debug

include ../Makefile.inc
LDFLAGS += $(call extract_funcs, pmem_log_get_treshold.c)
