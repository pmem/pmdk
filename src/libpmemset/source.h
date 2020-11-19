/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

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

	MAX_PMEMSET_SOURCE_TYPE
};

#ifdef _WIN32
int pmemset_source_extract(struct pmemset_source *src, HANDLE *handle);
#else
int pmemset_source_extract(struct pmemset_source *src, int *fd);
#endif
int pmemset_source_validate(const struct pmemset_source *src);

int pmemset_source_create_pmemset_file(struct pmemset_source *src,
		struct pmemset_file **file, struct pmemset_config *cfg);

#endif /* PMEMSET_SOURCE_H */
