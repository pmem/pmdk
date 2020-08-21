/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_utils.h -- libpmem2 utilities functions
 */

#ifndef PMEM2_UTILS_H
#define PMEM2_UTILS_H 1

#include <errno.h>

#include "os.h"
#include "out.h"
#include "source.h"

static inline int
pmem2_assert_errno(void)
{
	if (!errno) {
		ERR("errno is not set");
		ASSERTinfo(0, "errno is not set");
		return -EINVAL;
	}

	return -errno;
}

#define PMEM2_E_ERRNO (pmem2_assert_errno())

void *pmem2_malloc(size_t size, int *err);
void *pmem2_zalloc(size_t size, int *err);
void *pmem2_realloc(void *ptr, size_t size, int *err);

#ifdef _WIN32
int pmem2_lasterror_to_err();
#endif

int pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type);
int pmem2_device_dax_size(const struct pmem2_source *src, size_t *size);
int pmem2_device_dax_alignment(const struct pmem2_source *src,
		size_t *alignment);

#endif /* PMEM2_UTILS_H */
