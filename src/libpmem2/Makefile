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
	$(COMMON)/alloc.c\
	$(COMMON)/fs_posix.c\
	$(COMMON)/os_posix.c\
	$(COMMON)/os_thread_posix.c\
	$(COMMON)/out.c\
	$(COMMON)/ravl.c\
	$(COMMON)/util.c\
	$(COMMON)/util_posix.c\
	libpmem2.c\
	config.c\
	config_posix.c\
	errormsg.c\
	memops_generic.c\
	pmem2.c\
	map.c\
	map_posix.c\
	persist.c\
	persist_posix.c\
	pmem2_utils.c

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE +=\
	pmem2_utils_linux.c\
	auto_flush_linux.c
else
SOURCE +=\
	pmem2_utils_other.c\
	auto_flush_none.c
endif

include $(ARCH)/sources.inc

SOURCE += $(LIBPMEM2_ARCH_SOURCE)

include ../Makefile.inc

include $(PMEM2)/$(ARCH)/flags.inc

CFLAGS += -I.
LIBS += -pthread
