# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation

#
# src/libpmemblk/Makefile -- Makefile for libpmemblk
#

LIBRARY_NAME = pmemblk
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0

include ../core/pmemcore.inc
include ../common/pmemcommon.inc
SOURCE +=\
	blk.c\
	btt.c\
	libpmemblk.c

include ../Makefile.inc

CFLAGS += $(LIBNDCTL_CFLAGS) $(MINIASYNC_CFLAGS) -DPMEM2_USE_MINIASYNC=1
LIBS += -pthread -lpmem $(LIBNDCTL_LIBS)
