# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
# src/pmemcommon.inc -- common SOURCE definitions for PMDK libraries
#

SOURCE +=\
	$(COMMON)/badblock_$(OS_DIMM).c\
	$(COMMON)/set_badblocks.c\
	$(COMMON)/ctl.c\
	$(COMMON)/ctl_prefault.c\
	$(COMMON)/ctl_sds.c\
	$(COMMON)/ctl_fallocate.c\
	$(COMMON)/ctl_cow.c\
	$(call osdep, $(COMMON)/extent,.c)\
	$(COMMON)/file.c\
	$(COMMON)/file_posix.c\
	$(COMMON)/mmap.c\
	$(COMMON)/mmap_posix.c\
	$(COMMON)/os_deep_linux.c\
	$(COMMON)/pool_hdr.c\
	$(COMMON)/rand.c\
	$(COMMON)/ravl.c\
	$(COMMON)/set.c\
	$(COMMON)/shutdown_state.c\
	$(COMMON)/uuid.c\
	$(call osdep, $(COMMON)/uuid,.c)\
	$(PMEM2)/pmem2_utils.c\
	$(PMEM2)/config.c\
	$(PMEM2)/persist_posix.c\
	$(PMEM2)/badblocks.c\
	$(PMEM2)/badblocks_$(OS_DIMM).c\
	$(PMEM2)/usc_$(OS_DIMM).c\
	$(PMEM2)/source.c\
	$(PMEM2)/source_posix.c

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE +=\
	$(PMEM2)/auto_flush_linux.c\
	$(PMEM2)/deep_sync_linux.c\
	$(PMEM2)/pmem2_utils_linux.c
else
SOURCE +=\
	$(PMEM2)/auto_flush_none.c\
	$(PMEM2)/deep_sync_other.c\
	$(PMEM2)/pmem2_utils_other.c
endif

ifeq ($(OS_DIMM),ndctl)
SOURCE += $(PMEM2)/ndctl_region_namespace.c
endif
