
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

#
# src/libpmemset/Makefile -- Makefile for libpmemset
#

include ../common.inc

LIBRARY_NAME = pmemset
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE =\
	config.c\
	errormsg.c\
	libpmemset.c\
	part.c\
	pmemset.c\
	pmemset_utils.c\
	source.c

include ../core/pmemcore.inc
include ../Makefile.inc

CFLAGS += -I.
LIBS += -pthread -lpmem2
