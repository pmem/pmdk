# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2019, Intel Corporation

#
# src/libpmem/Makefile -- Makefile for libpmem
#

include ../common.inc

LIBRARY_NAME = pmem
LIBRARY_SO_VERSION = 1
LIBRARY_VERSION = 0.0
SOURCE =\
	$(COMMON)/alloc.c\
	$(COMMON)/file.c\
	$(COMMON)/file_posix.c\
	$(COMMON)/fs_posix.c\
	$(COMMON)/mmap.c\
	$(COMMON)/mmap_posix.c\
	$(COMMON)/os_posix.c\
	$(COMMON)/os_thread_posix.c\
	$(COMMON)/os_deep_linux.c\
	$(COMMON)/out.c\
	$(COMMON)/util.c\
	$(COMMON)/util_posix.c\
	libpmem.c\
	memops_generic.c\
	pmem.c\
	pmem_posix.c\
	$(PMEM2)/pmem2_utils.c\
	$(PMEM2)/config.c\
	$(PMEM2)/config_posix.c

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE +=\
	$(PMEM2)/pmem2_utils_linux.c\
	$(PMEM2)/auto_flush_linux.c
else
SOURCE +=\
	$(PMEM2)/pmem2_utils_other.c\
	$(PMEM2)/auto_flush_none.c
endif

INCS += -I$(TOP)/src/libpmem2

include ../libpmem2/$(ARCH)/sources.inc

SOURCE += $(LIBPMEM2_ARCH_SOURCE)

include ../Makefile.inc

include $(PMEM2)/$(ARCH)/flags.inc

CFLAGS += -I.
LIBS += -pthread
