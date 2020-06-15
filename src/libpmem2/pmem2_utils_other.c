// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

#include <errno.h>
#include <sys/stat.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"

#ifdef _WIN32
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif

int
pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type)
{
	if (S_ISREG(st->st_mode)) {
		*type = PMEM2_FTYPE_REG;
		return 0;
	}

	if (S_ISDIR(st->st_mode)) {
		*type = PMEM2_FTYPE_DIR;
		return 0;
	}

	ERR("file type 0%o not supported", st->st_mode & S_IFMT);
	return PMEM2_E_INVALID_FILE_TYPE;
}

/*
 * pmem2_device_dax_size -- checks the size of a given
 * dax device from given source structure
 */
int
pmem2_device_dax_size(const struct pmem2_source *src, size_t *size)
{
	const char *err =
		"BUG: pmem2_device_dax_size should never be called on this OS";
	ERR("%s", err);
	ASSERTinfo(0, err);
	return PMEM2_E_NOSUPP;
}

/*
 * pmem2_device_dax_alignment -- checks the alignment of a given
 * dax device from given source
 */
int
pmem2_device_dax_alignment(const struct pmem2_source *src, size_t *alignment)
{
	const char *err =
		"BUG: pmem2_device_dax_alignment should never be called on this OS";
	ERR("%s", err);
	ASSERTinfo(0, err);
	return PMEM2_E_NOSUPP;
}
