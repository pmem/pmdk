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
# This flag allows to build tests and examples
# using pmemlog despite the deprecated state.
CFLAGS += -Wno-deprecated-declarations

CFLAGS += $(LIBNDCTL_CFLAGS)

LIBS += -pthread -lpmem $(LIBNDCTL_LIBS)
