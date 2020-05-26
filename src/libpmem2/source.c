// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

int
pmem2_source_from_anon(struct pmem2_source **src, size_t size)
{
	int ret;
	struct pmem2_source *srcp = pmem2_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEM2_SOURCE_ANON;
	srcp->value.size = size;

	*src = srcp;

	return 0;
}

int
pmem2_source_delete(struct pmem2_source **src)
{
	Free(*src);
	*src = NULL;
	return 0;
}
