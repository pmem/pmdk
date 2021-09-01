/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020-2021, Intel Corporation */

/*
 * file.h -- internal definitions for libpmemset file API
 */
#ifndef PMEMSET_FILE_H
#define PMEMSET_FILE_H

#include <stdbool.h>

#include "libpmemset.h"
#include "source.h"

struct pmemset_file;

int pmemset_file_from_file(struct pmemset_file **file, char *path,
		uint64_t flags);

int pmemset_file_from_pmem2(struct pmemset_file **file,
		struct pmem2_source *pmem2_src);

int pmemset_file_from_dir(struct pmemset_file **file, char *dir);

void pmemset_file_delete(struct pmemset_file **file);

struct pmem2_source *pmemset_file_get_pmem2_source(struct pmemset_file *file);

int pmemset_file_create_pmem2_src(struct pmem2_source **pmem2_src, char *path,
		uint64_t flags);

int pmemset_file_create_pmem2_src_from_temp(struct pmem2_source **pmem2_src,
		char *dir);

int pmemset_file_dispose_pmem2_src(struct pmem2_source **pmem2_src);

int pmemset_file_close(struct pmem2_source *pmem2_src);

bool pmemset_file_get_grow(struct pmemset_file *file);

int pmemset_file_grow(struct pmemset_file *file, size_t len);

#ifndef _WIN32
int pmemset_file_get_fd(struct pmemset_file *file);
#else
HANDLE pmemset_file_get_handle(struct pmemset_file *file);
#endif

#endif /* PMEMSET_FILE_H */
