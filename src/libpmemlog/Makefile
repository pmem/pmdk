# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/libpmemlog/Makefile -- Makefile for libpmemlog
#

LIBRARY_NAME = pmemlog
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0

include ../core/pmemcore.inc
include ../common/pmemcommon.inc

SOURCE +=\
	libpmemlog.c\
	log.c

include ../Makefile.inc

# Libpmemlog is deprecated.
# This flag allows to build tests, examples and benchmarks
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

CFLAGS += $(LIBNDCTL_CFLAGS) $(MINIASYNC_CFLAGS) -DPMEM2_USE_MINIASYNC=1

LIBS += -pthread -lpmem $(LIBNDCTL_LIBS)
