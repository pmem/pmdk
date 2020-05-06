// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#ifndef PMEM2_SOURCE_H
#define PMEM2_SOURCE_H

#include "pmem2_utils.h"

#define INVALID_FD (-1)

enum pmem2_source_type {
	PMEM2_SOURCE_UNSPECIFIED,
	PMEM2_SOURCE_ANON,
	PMEM2_SOURCE_FD,
	PMEM2_SOURCE_HANDLE,

	MAX_PMEM2_SOURCE_TYPE
};

struct pmem2_source {
	/* a source file descriptor / handle for the designed mapping */
	enum pmem2_source_type type;
	struct {
		enum pmem2_file_type file;
		union {
#ifdef _WIN32
			HANDLE handle;
#else
			struct {
				int fd;
				os_stat_t st;
			};
#endif
		};
	} value;
};

#endif /* PMEM2_SOURCE_H */
