// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2022, Intel Corporation */

#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

int
pmem2_source_from_anon(struct pmem2_source **src, size_t size)
{
	PMEM2_ERR_CLR();

	int ret;
	struct pmem2_source *srcp = pmem2_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEM2_SOURCE_ANON;
	srcp->value.size = size;

	*src = srcp;

	return 0;
}

/*
 * pmem2_source_from_existing -- create a source from an existing virtual
 *                               memory mapping
 */
int
pmem2_source_from_existing(struct pmem2_source **src, void *addr, size_t size,
		int is_pmem)
{
	PMEM2_ERR_CLR();

	int ret;
	struct pmem2_source *srcp = pmem2_malloc(sizeof(*srcp), &ret);
	if (ret)
		return ret;

	srcp->type = PMEM2_SOURCE_EXISTING;
	srcp->value.existing.addr = addr;
	srcp->value.existing.size = size;
	srcp->value.existing.is_pmem = is_pmem == 1 ? 1 : 0;

	*src = srcp;

	return 0;
}

int
pmem2_source_delete(struct pmem2_source **src)
{
	/* we do not need to clear err because this function cannot fail */

	Free(*src);
	*src = NULL;
	return 0;
}
