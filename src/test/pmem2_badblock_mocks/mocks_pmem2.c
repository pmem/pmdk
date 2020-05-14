// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_pmem2.c -- mocked pmem2 functions used
 *                  indirectly in pmem2_badblock_mocks.c
 */

#include "unittest.h"
#include "out.h"
#include "extent.h"
#include "pmem2_utils.h"
#include "pmem2_badblock_mocks.h"

/*
 * pmem2_get_type_from_stat - mock pmem2_get_type_from_stat
 */
FUNC_MOCK(pmem2_get_type_from_stat, int,
	const os_stat_t *st, enum pmem2_file_type *type)
FUNC_MOCK_RUN_DEFAULT {

	if (S_ISREG(st->st_mode)) {
		*type = PMEM2_FTYPE_REG;
		return 0;
	}

	if (S_ISDIR(st->st_mode)) {
		*type = PMEM2_FTYPE_DIR;
		return 0;
	}

	if (S_ISCHR(st->st_mode)) {
		*type = PMEM2_FTYPE_DEVDAX;
		return 0;
	}

	ERR("file type 0%o not supported", st->st_mode & S_IFMT);
	return PMEM2_E_INVALID_FILE_TYPE;
}
FUNC_MOCK_END

/*
 * pmem2_extents_create_get -- allocate extents structure and get extents
 *                             of the given file
 */
FUNC_MOCK(pmem2_extents_create_get, int,
		int fd, struct extents **exts)
FUNC_MOCK_RUN_DEFAULT {
	return get_extents(fd, exts);
}
FUNC_MOCK_END
