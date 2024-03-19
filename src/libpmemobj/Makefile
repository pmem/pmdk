# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2024, Intel Corporation

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
	obj_log.c\
	palloc.c\
	pmalloc.c\
	recycler.c\
	sync.c\
	tx.c\
	stats.c\
	ulog.c

include ../Makefile.inc

ifeq ($(OS_DIMM),none)
	NOT_RECOMMENDED = Continuing the build without NDCTL is highly NOT recommended for production quality systems.
	RAS_SUFFIX = Please see https://www.intel.com/content/www/us/en/developer/articles/technical/build-pmem-apps-with-ras.html for more info on RAS features.
        PMEMOBJ_IGNORE_DIRTY_SHUTDOWN ?= n
        ifneq ($(PMEMOBJ_IGNORE_DIRTY_SHUTDOWN),y)
                $(info Libpmemobj without NDCTL cannot detect dirty shutdowns. This may result in silent data corruption.)
                $(info $(NOT_RECOMMENDED))
                $(info If you understand the consequences for consistency of your data please set PMEMOBJ_IGNORE_DIRTY_SHUTDOWN=y to silence this error.)
                $(info $(RAS_SUFFIX))
                $(error )
        endif

        PMEMOBJ_IGNORE_BAD_BLOCKS ?= n
        ifneq ($(PMEMOBJ_IGNORE_BAD_BLOCKS),y)
                $(info Libpmemobj without NDCTL cannot detect bad blocks up front. This may result in SIGBUS at runtime.)
                $(info $(NOT_RECOMMENDED))
                $(info If you understand the consequences for the behaviour of your application at runtime please set PMEMOBJ_IGNORE_BAD_BLOCKS=y to silence this error.)
                $(info $(RAS_SUFFIX))
                $(error )
        endif
endif

CFLAGS += -DUSE_LIBDL -D_PMEMOBJ_INTRNL $(LIBNDCTL_CFLAGS)

LIBS += -pthread -lpmem $(LIBDL) $(LIBNDCTL_LIBS)
