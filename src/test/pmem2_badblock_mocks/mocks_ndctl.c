// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * mocks_ndctl.c -- mocked ndctl functions used in pmem2_badblock_mocks.c
 */

#include <sys/stat.h>
#include <ndctl/libndctl.h>

#include "unittest.h"
#include "pmem2_badblock_mocks.h"

#define RESOURCE_ADDRESS	0x1000 /* any non-zero value */

#define UINT(ptr) (unsigned)((uintptr_t)ptr)

/* index of bad blocks */
static unsigned i_bb;

/*
 * ndctl_region_namespace - mock ndctl_region_namespace
 *
 * st->st_ino ==  0 - did not found any matching device
 * st->st_ino <  50 - namespace mode
 * st->st_ino >= 50 - region mode
 */
FUNC_MOCK(ndctl_region_namespace, int,
		struct ndctl_ctx *ctx,
		const os_stat_t *st,
		struct ndctl_region **pregion,
		struct ndctl_namespace **pndns)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTne(pregion, NULL);

	if (st->st_ino == MODE_NO_DEV) {
		/* did not found any matching device */
		*pregion = NULL;
		return 0;
	}

	/*
	 * st->st_ino <  50 - namespace mode
	 * st->st_ino >= 50 - region mode
	 */
	*pregion = (void *)st->st_ino;

	if (pndns == NULL)
		return 0;

	/*
	 * st->st_ino <  50 - namespace mode
	 * st->st_ino >= 50 - region mode
	 */
	*pndns = (void *)st->st_ino;

	return 0;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_mode - mock ndctl_namespace_get_mode
 *
 * ndns <  50 - namespace mode
 * ndns >= 50 - region mode
 */
FUNC_MOCK(ndctl_namespace_get_mode, enum ndctl_namespace_mode,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	if ((uintptr_t)ndns >= (uintptr_t)MODE_REGION) {
		/* region mode */
		return NDCTL_NS_MODE_RAW;
	}

	/* namespace mode */
	return NDCTL_NS_MODE_FSDAX;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_pfn - mock ndctl_namespace_get_pfn
 *
 * ndns <  50 - namespace mode
 * ndns >= 50 - region mode
 */
FUNC_MOCK(ndctl_namespace_get_pfn, struct ndctl_pfn *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	if ((uintptr_t)ndns < (uintptr_t)MODE_REGION)
		return (struct ndctl_pfn *)ndns;
	return NULL;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_dax - mock ndctl_namespace_get_dax
 *
 * ndns <  50 - namespace mode
 * ndns >= 50 - region mode
 */
FUNC_MOCK(ndctl_namespace_get_dax, struct ndctl_dax *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	if ((uintptr_t)ndns >= (uintptr_t)MODE_REGION)
		return (struct ndctl_dax *)ndns;
	return NULL;
}
FUNC_MOCK_END

/*
 * ndctl_dax_get_resource - mock ndctl_dax_get_resource
 */
FUNC_MOCK(ndctl_dax_get_resource, unsigned long long,
		struct ndctl_dax *dax)
FUNC_MOCK_RUN_DEFAULT {
	return RESOURCE_ADDRESS;
}
FUNC_MOCK_END

/*
 * ndctl_dax_get_size - mock ndctl_dax_get_size
 */
FUNC_MOCK(ndctl_dax_get_size, unsigned long long,
		struct ndctl_dax *dax)
FUNC_MOCK_RUN_DEFAULT {
	return DEV_SIZE_1GB; /* 1 GiB */
}
FUNC_MOCK_END

/*
 * ndctl_region_get_resource - mock ndctl_region_get_resource
 */
FUNC_MOCK(ndctl_region_get_resource, unsigned long long,
		struct ndctl_region *region)
FUNC_MOCK_RUN_DEFAULT {
	return RESOURCE_ADDRESS;
}
FUNC_MOCK_END

/*
 * ndctl_region_get_bus - mock ndctl_region_get_bus
 */
FUNC_MOCK(ndctl_region_get_bus, struct ndctl_bus *,
		struct ndctl_region *region)
FUNC_MOCK_RUN_DEFAULT {
	return (struct ndctl_bus *)region;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_first_badblock - mock ndctl_namespace_get_first_badblock
 */
FUNC_MOCK(ndctl_namespace_get_first_badblock, struct badblock *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	i_bb = 0;
	return get_next_hw_badblock(UINT(ndns), &i_bb);
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_next_badblock - mock ndctl_namespace_get_next_badblock
 */
FUNC_MOCK(ndctl_namespace_get_next_badblock, struct badblock *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	return get_next_hw_badblock(UINT(ndns), &i_bb);
}
FUNC_MOCK_END

/*
 * ndctl_region_get_first_badblock - mock ndctl_region_get_first_badblock
 */
FUNC_MOCK(ndctl_region_get_first_badblock, struct badblock *,
		struct ndctl_region *region)
FUNC_MOCK_RUN_DEFAULT {
	i_bb = 0;
	return get_next_hw_badblock(UINT(region), &i_bb);
}
FUNC_MOCK_END

/*
 * ndctl_region_get_next_badblock - mock ndctl_region_get_next_badblock
 */
FUNC_MOCK(ndctl_region_get_next_badblock, struct badblock *,
		struct ndctl_region *region)
FUNC_MOCK_RUN_DEFAULT {
	return get_next_hw_badblock(UINT(region), &i_bb);
}
FUNC_MOCK_END
