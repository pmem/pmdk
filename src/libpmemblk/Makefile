# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

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

# Libpmemblk is deprecated.
# This flag allows to build tests and examples
# using pmemblk despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += -pthread -lpmem $(LIBNDCTL_LIBS)
