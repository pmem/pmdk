/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * file.h -- internal definitions for libpmemset file API
 */
#ifndef PMEMSET_FILE_H
#define PMEMSET_FILE_H

#include <stdbool.h>

#include "libpmemset.h"

struct pmemset_file {
	bool close;
	struct {
		struct pmem2_source *src;
	} pmem2;
	union {
#ifdef _WIN32
		HANDLE handle;
#else
		int fd;
#endif
	};
};

int pmemset_file_from_file(struct pmemset_file **file, char *path,
		struct pmemset_config *cfg);

int pmemset_file_from_pmem2(struct pmemset_file **file,
		struct pmem2_source *pmem2_src);

void pmemset_file_delete(struct pmemset_file **file);

int pmemset_file_close(struct pmemset_file *file);

struct pmem2_source *pmemset_file_get_pmem2_source(struct pmemset_file *file);

#endif /* PMEMSET_FILE_H */
