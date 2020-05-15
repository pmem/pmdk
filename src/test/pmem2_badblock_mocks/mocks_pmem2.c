// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_pmem2.c -- mocked pmem2 functions used
 *                  indirectly in pmem2_badblock_mocks.c
 */

#include <ndctl/libndctl.h>
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
 * pmem2_region_namespace - mock pmem2_region_namespace
 */
FUNC_MOCK(pmem2_region_namespace, int,
		struct ndctl_ctx *ctx,
		const os_stat_t *st,
		struct ndctl_region **pregion,
		struct ndctl_namespace **pndns)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTne(pregion, NULL);
	*pregion = (void *)st->st_ino;
	if (pndns == NULL)
		return 0;

	if (IS_MODE_NO_DEVICE(st->st_ino) || /* no matching device */
	    st->st_mode == __S_IFDIR || /* directory */
	    st->st_mode == __S_IFBLK) { /* block device */
		/* did not found any matching device */
		*pndns = NULL;
		return 0;
	}

	*pndns = (void *)st->st_ino;

	return 0;
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
