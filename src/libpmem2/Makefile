# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

#
# src/libpmem2/Makefile -- Makefile for libpmem2
#

include ../common.inc

LIBRARY_NAME = pmem2
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE =\
	$(COMMON)/ravl.c\
	libpmem2.c\
	badblocks.c\
	badblocks_$(OS_DIMM).c\
	config.c\
	deep_sync.c\
	errormsg.c\
	memops_generic.c\
	map.c\
	map_posix.c\
	persist.c\
	persist_posix.c\
	pmem2.c\
	pmem2_utils.c\
	usc_$(OS_DIMM).c\
	source.c\
	source_posix.c\
	vm_reservation.c

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE +=\
	auto_flush_linux.c\
	deep_sync_linux.c\
	extent_linux.c\
	pmem2_utils_linux.c
else
SOURCE +=\
	auto_flush_none.c\
	deep_sync_other.c\
	extent_none.c\
	pmem2_utils_other.c
endif

ifeq ($(OS_DIMM),ndctl)
SOURCE += ndctl_region_namespace.c
endif

include $(ARCH)/sources.inc

include ../core/pmemcore.inc

SOURCE += $(LIBPMEM2_ARCH_SOURCE)

include ../Makefile.inc

include $(PMEM2)/$(ARCH)/flags.inc

CFLAGS += -I. $(LIBNDCTL_CFLAGS)
LIBS += -pthread $(LIBNDCTL_LIBS)
