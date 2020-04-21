// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_utils.h -- libpmem2 utilities functions
 */

#ifndef PMEM2_UTILS_H
#define PMEM2_UTILS_H 1

#include <errno.h>

#include "os.h"

#define PMEM2_E_ERRNO (-errno)

void *pmem2_malloc(size_t size, int *err);
void *pmem2_zalloc(size_t size, int *err);
void *pmem2_realloc(void *ptr, size_t size, int *err);

#ifdef _WIN32
int pmem2_lasterror_to_err();
#endif

enum pmem2_file_type {
	PMEM2_FTYPE_REG = 1,
	PMEM2_FTYPE_DEVDAX = 2,
	PMEM2_FTYPE_DIR = 3,
};

int pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type);
int pmem2_device_dax_size_from_stat(const os_stat_t *st, size_t *size);
int pmem2_device_dax_alignment_from_stat(const os_stat_t *st,
		size_t *alignment);
int pmem2_device_dax_region_find(const os_stat_t *st);

#endif /* PMEM2_UTILS_H */
