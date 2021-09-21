/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * source.h -- internal definitions for pmemset_source
 */
#ifndef PMEMSET_SOURCE_H
#define PMEMSET_SOURCE_H

#include "file.h"

enum pmemset_source_type {
	PMEMSET_SOURCE_UNSPECIFIED,
	PMEMSET_SOURCE_PMEM2,
	PMEMSET_SOURCE_FILE,
	PMEMSET_SOURCE_TEMP,

	MAX_PMEMSET_SOURCE_TYPE
};

int pmemset_source_validate(const struct pmemset_source *src);

struct pmemset_file *pmemset_source_get_set_file(struct pmemset_source *src);

int pmemset_source_get_pmem2_source(struct pmemset_source *src,
	struct pmem2_source **pmem2_src);

int pmemset_source_destroy_pmem2_source(struct pmemset_source *src,
	struct pmem2_source **pmem2_src);

int pmemset_source_get_pmem2_map_from_file(const struct pmemset_source *src,
		struct pmem2_config *cfg, struct pmem2_map **map);

int pmemset_source_get_pmem2_map_from_src(const struct pmemset_source *src,
		struct pmem2_config *cfg, struct pmem2_map **map);

int pmemset_source_create_pmemset_file(struct pmemset_source *src,
		struct pmemset_file **file, uint64_t flags);

size_t pmemset_source_get_offset(struct pmemset_source *src);

size_t pmemset_source_get_length(struct pmemset_source *src);

struct pmemset_sds *pmemset_source_get_sds(struct pmemset_source *src);

enum pmemset_part_state *pmemset_source_get_part_state_ptr(
			struct pmemset_source *src);

int pmemset_source_get_use_count(struct pmemset_source *src);

void pmemset_source_increment_use_count(struct pmemset_source *src);

void pmemset_source_decrement_use_count(struct pmemset_source *src);

#endif /* PMEMSET_SOURCE_H */
