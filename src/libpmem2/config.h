// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * config.h -- internal definitions for pmem2_config
 */
#ifndef PMEM2_CONFIG_H
#define PMEM2_CONFIG_H

#include "libpmem2.h"

#define PMEM2_GRANULARITY_INVALID ((enum pmem2_granularity) (-1))

struct pmem2_config {
	/* offset from the beginning of the file */
	size_t offset;
	size_t length; /* length of the mapping */
	/* persistence granularity requested by user */
	enum pmem2_granularity requested_max_granularity;
	enum pmem2_sharing_type sharing; /* the way the file will be mapped */
};

void pmem2_config_init(struct pmem2_config *cfg);

int pmem2_config_validate_length(const struct pmem2_config *cfg,
		size_t file_len, size_t alignment);

#endif /* PMEM2_CONFIG_H */
