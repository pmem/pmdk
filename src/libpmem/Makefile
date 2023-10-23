# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# src/libpmem/Makefile -- Makefile for libpmem
#

include ../common.inc

LIBRARY_NAME = pmem
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE =\
	$(CORE)/alloc.c\
	$(CORE)/fs_posix.c\
	$(CORE)/os_posix.c\
	$(CORE)/os_thread_posix.c\
	$(CORE)/out.c\
	$(CORE)/util.c\
	$(CORE)/util_posix.c\
	$(COMMON)/file.c\
	$(COMMON)/file_posix.c\
	$(COMMON)/mmap.c\
	$(COMMON)/mmap_posix.c\
	$(COMMON)/os_deep_linux.c\
	libpmem.c\
	memops_generic.c\
	pmem.c\
	pmem_posix.c\
	$(PMEM2)/pmem2_utils.c\
	$(PMEM2)/config.c\
	$(PMEM2)/persist_posix.c\
	$(PMEM2)/source.c\
	$(PMEM2)/source_posix.c

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE +=\
	$(PMEM2)/pmem2_utils_linux.c\
	$(PMEM2)/pmem2_utils_$(OS_DIMM).c\
	$(PMEM2)/auto_flush_linux.c\
	$(PMEM2)/deep_flush_linux.c
else
SOURCE +=\
	$(PMEM2)/pmem2_utils_other.c\
	$(PMEM2)/auto_flush_none.c\
	$(PMEM2)/deep_flush_other.c
endif

ifeq ($(OS_DIMM),ndctl)
SOURCE +=\
	region_namespace_ndctl.c\
	numa_ndctl.c
else
SOURCE +=\
	region_namespace_none.c\
	numa_none.c
endif

INCS += -I$(TOP)/src/libpmem2

include ../libpmem2/$(ARCH)/sources.inc

SOURCE += $(LIBPMEM2_ARCH_SOURCE)

include ../Makefile.inc

include $(PMEM2)/$(ARCH)/flags.inc

CFLAGS += -I. $(LIBNDCTL_CFLAGS)
LIBS += -pthread $(LIBNDCTL_LIBS)
