# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

#
# src/test/pmreorder_flushes/Makefile -- build pmreorder_flushes test
#

TARGET = pmreorder_flushes
OBJS = pmreorder_flushes.o

LIBPMEM=y

# included for VALGRIND_EMIT_LOG
LIBPMEMCOMMON=y

include ../Makefile.inc
ifeq ($(ARCH), ppc64)
CFLAGS += -fsigned-char
endif
