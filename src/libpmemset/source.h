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

	MAX_PMEMSET_SOURCE_TYPE
};

struct pmemset_source {
	enum pmemset_source_type type;
	struct {
		union {
			struct pmem2_source *pmem2_src;
		};
	} value;
};

#endif /* PMEMSET_SOURCE_H */
