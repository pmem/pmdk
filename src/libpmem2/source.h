/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021-2023, Intel Corporation */

#ifndef PMEM2_SOURCE_H
#define PMEM2_SOURCE_H

#include "os.h"

#define INVALID_FD (-1)

enum pmem2_file_type {
	PMEM2_FTYPE_REG = 1,
	PMEM2_FTYPE_DEVDAX = 2,
	PMEM2_FTYPE_DIR = 3,

	MAX_PMEM2_FILE_TYPE = 4,
};

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
		enum pmem2_file_type ftype;
		union {
			/* PMEM2_SOURCE_ANON */
			size_t size;
			/* PMEM2_SOURCE_FD */
			struct {
				int fd;
				dev_t st_rdev;
				dev_t st_dev;
			};
		};
	} value;
};

#endif /* PMEM2_SOURCE_H */
