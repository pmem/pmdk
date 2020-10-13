/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * source.h -- internal definitions for pmemset_source
 */
#ifndef PMEMSET_SOURCE_H
#define PMEMSET_SOURCE_H

enum pmemset_source_type {
	PMEMSET_SOURCE_UNSPECIFIED,
	PMEMSET_SOURCE_PMEM2,
	PMEMSET_SOURCE_PATH,

	MAX_PMEMSET_SOURCE_TYPE
};

int pmemset_source_get_filepath(const struct pmemset_source *src,
		char **filepath);

int pmemset_source_get_pmem2_source(const struct pmemset_source *src,
		struct pmem2_source **pmem2_src);

enum pmemset_source_type pmemset_source_get_type(
		const struct pmemset_source *src);

#endif /* PMEMSET_SOURCE_H */
