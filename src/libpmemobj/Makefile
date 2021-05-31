# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2021, Intel Corporation

#
# src/libpmemobj/Makefile -- Makefile for libpmemobj
#

LIBRARY_NAME = pmemobj
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0

include ../core/pmemcore.inc
include ../common/pmemcommon.inc

SOURCE +=\
	alloc_class.c\
	arenas.c\
	bucket.c\
	container_ravl.c\
	container_seglists.c\
	critnib.c\
	ctl_debug.o\
	heap.c\
	lane.c\
	libpmemobj.c\
	list.c\
	memblock.c\
	memops.c\
	obj.c\
	palloc.c\
	pmalloc.c\
	recycler.c\
	sync.c\
	tx.c\
	stats.c\
	ulog.c

include ../Makefile.inc

CFLAGS += -DUSE_LIBDL -D_PMEMOBJ_INTRNL $(LIBNDCTL_CFLAGS)

LIBS += -pthread -lpmem $(LIBDL) $(LIBNDCTL_LIBS)
