// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_ndctl.c -- mocked ndctl functions used
 *                  indirectly in pmem2_badblock_mocks.c
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
 * ndctl_namespace_get_mode - mock ndctl_namespace_get_mode
 */
FUNC_MOCK(ndctl_namespace_get_mode, enum ndctl_namespace_mode,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	if (IS_MODE_NAMESPACE((uintptr_t)ndns))
		/* namespace mode */
		return NDCTL_NS_MODE_FSDAX;

	/* raw mode */
	return NDCTL_NS_MODE_RAW;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_pfn - mock ndctl_namespace_get_pfn
 */
FUNC_MOCK(ndctl_namespace_get_pfn, struct ndctl_pfn *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	if (IS_MODE_NAMESPACE((uintptr_t)ndns))
		/* namespace mode */
		return (struct ndctl_pfn *)ndns;
	return NULL;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_dax - mock ndctl_namespace_get_dax
 */
FUNC_MOCK(ndctl_namespace_get_dax, struct ndctl_dax *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	if (IS_MODE_REGION((uintptr_t)ndns))
		/* region mode */
		return (struct ndctl_dax *)ndns;
	return NULL;
}
FUNC_MOCK_END

/*
 * ndctl_pfn_get_resource - mock ndctl_pfn_get_resource
 */
FUNC_MOCK(ndctl_pfn_get_resource, unsigned long long,
		struct ndctl_pfn *pfn)
FUNC_MOCK_RUN_DEFAULT {
	return RESOURCE_ADDRESS;
}
FUNC_MOCK_END

/*
 * ndctl_pfn_get_size - mock ndctl_pfn_get_size
 */
FUNC_MOCK(ndctl_pfn_get_size, unsigned long long,
		struct ndctl_pfn *pfn)
FUNC_MOCK_RUN_DEFAULT {
	return DEV_SIZE_1GB; /* 1 GiB */
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
 * ndctl_namespace_get_resource - mock ndctl_namespace_get_resource
 */
FUNC_MOCK(ndctl_namespace_get_resource, unsigned long long,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	return RESOURCE_ADDRESS;
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_size - mock ndctl_namespace_get_size
 */
FUNC_MOCK(ndctl_namespace_get_size, unsigned long long,
		struct ndctl_namespace *ndns)
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
	return get_nth_hw_badblock(UINT(ndns), &i_bb);
}
FUNC_MOCK_END

/*
 * ndctl_namespace_get_next_badblock - mock ndctl_namespace_get_next_badblock
 */
FUNC_MOCK(ndctl_namespace_get_next_badblock, struct badblock *,
		struct ndctl_namespace *ndns)
FUNC_MOCK_RUN_DEFAULT {
	return get_nth_hw_badblock(UINT(ndns), &i_bb);
}
FUNC_MOCK_END

/*
 * ndctl_region_get_first_badblock - mock ndctl_region_get_first_badblock
 */
FUNC_MOCK(ndctl_region_get_first_badblock, struct badblock *,
		struct ndctl_region *region)
FUNC_MOCK_RUN_DEFAULT {
	i_bb = 0;
	return get_nth_hw_badblock(UINT(region), &i_bb);
}
FUNC_MOCK_END

/*
 * ndctl_region_get_next_badblock - mock ndctl_region_get_next_badblock
 */
FUNC_MOCK(ndctl_region_get_next_badblock, struct badblock *,
		struct ndctl_region *region)
FUNC_MOCK_RUN_DEFAULT {
	return get_nth_hw_badblock(UINT(region), &i_bb);
}
FUNC_MOCK_END

static struct ndctl_data {
	uintptr_t bus;
	unsigned long long address;
	unsigned long long length;
} data;

/*
 * ndctl_bus_cmd_new_ars_cap - mock ndctl_bus_cmd_new_ars_cap
 */
FUNC_MOCK(ndctl_bus_cmd_new_ars_cap, struct ndctl_cmd *,
		struct ndctl_bus *bus, unsigned long long address,
		unsigned long long len)
FUNC_MOCK_RUN_DEFAULT {
	data.bus = (uintptr_t)bus;
	data.address = address;
	data.length = len;
	return (struct ndctl_cmd *)&data;
}
FUNC_MOCK_END

/*
 * ndctl_cmd_submit - mock ndctl_cmd_submit
 */
FUNC_MOCK(ndctl_cmd_submit, int, struct ndctl_cmd *cmd)
FUNC_MOCK_RUN_DEFAULT {
	return 0;
}
FUNC_MOCK_END

/*
 * ndctl_cmd_ars_cap_get_range - mock ndctl_cmd_ars_cap_get_range
 */
FUNC_MOCK(ndctl_cmd_ars_cap_get_range, int,
		struct ndctl_cmd *ars_cap, struct ndctl_range *range)
FUNC_MOCK_RUN_DEFAULT {
	return 0;
}
FUNC_MOCK_END

/*
 * ndctl_bus_cmd_new_clear_error - mock ndctl_bus_cmd_new_clear_error
 */
FUNC_MOCK(ndctl_bus_cmd_new_clear_error, struct ndctl_cmd *,
		unsigned long long address,
		unsigned long long len,
		struct ndctl_cmd *ars_cap)
FUNC_MOCK_RUN_DEFAULT {
	return ars_cap;
}
FUNC_MOCK_END

/*
 * ndctl_cmd_clear_error_get_cleared - mock ndctl_cmd_clear_error_get_cleared
 */
FUNC_MOCK(ndctl_cmd_clear_error_get_cleared, unsigned long long,
		struct ndctl_cmd *clear_err)
FUNC_MOCK_RUN_DEFAULT {
	struct ndctl_data *pdata = (struct ndctl_data *)clear_err;
	UT_OUT("ndctl_clear_error(%lu, %llu, %llu)",
		pdata->bus, pdata->address, pdata->length);
	return pdata->length;
}
FUNC_MOCK_END

/*
 * ndctl_cmd_unref - mock ndctl_cmd_unref
 */
FUNC_MOCK(ndctl_cmd_unref, void, struct ndctl_cmd *cmd)
FUNC_MOCK_RUN_DEFAULT {
}
FUNC_MOCK_END
