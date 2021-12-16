/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2022, Intel Corporation */

/*
 * config.h -- internal definitions for pmem2_config
 */
#ifndef PMEM2_CONFIG_H
#define PMEM2_CONFIG_H

#include "libpmem2.h"

#define PMEM2_GRANULARITY_INVALID ((enum pmem2_granularity) (-1))
#define PMEM2_ADDRESS_ANY 0 /* default value of the address request type */

struct pmem2_config {
	/* offset from the beginning of the file */
	size_t offset;
	size_t length; /* length of the mapping */
	/* persistence granularity requested by user */
	void *addr; /* address of the mapping */
	int addr_request; /* address request type */
	enum pmem2_granularity requested_max_granularity;
	enum pmem2_sharing_type sharing; /* the way the file will be mapped */
	unsigned protection_flag;
	struct pmem2_vm_reservation *reserv;
	size_t reserv_offset;
	struct vdm *vdm;
};

void pmem2_config_init(struct pmem2_config *cfg);

int pmem2_config_validate_length(const struct pmem2_config *cfg,
		size_t file_len, size_t alignment);

#endif /* PMEM2_CONFIG_H */
