# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
# src/pmemcommon.inc -- common SOURCE definitions for PMDK libraries
#

SOURCE +=\
	$(COMMON)/badblock_$(OS_DIMM).c\
	$(COMMON)/badblock.c\
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
	$(COMMON)/os_dimm_$(OS_DIMM).c\
	$(COMMON)/os_deep_linux.c\
	$(COMMON)/pool_hdr.c\
	$(COMMON)/rand.c\
	$(COMMON)/set.c\
	$(COMMON)/shutdown_state.c\
	$(COMMON)/uuid.c\
	$(call osdep, $(COMMON)/uuid,.c)\
	$(PMEM2)/pmem2_utils.c\
	$(PMEM2)/config.c\
	$(PMEM2)/source.c\
	$(PMEM2)/source_posix.c

ifeq ($(OS_KERNEL_NAME),Linux)
SOURCE +=\
	$(PMEM2)/pmem2_utils_linux.c\
	$(PMEM2)/auto_flush_linux.c
else
SOURCE +=\
	$(PMEM2)/auto_flush_none.c\
	$(PMEM2)/pmem2_utils_other.c
endif
