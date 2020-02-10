// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

int
pmem2_source_from_anon(struct pmem2_source **src)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_delete(struct pmem2_source **src)
{
	Free(*src);
	*src = NULL;
	return 0;
}

#ifndef _WIN32
int
pmem2_source_device_id(const struct pmem2_source *cfg,
	char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#else
int
pmem2_source_device_idW(const struct pmem2_source *cfg,
	wchar_t *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_device_idU(const struct pmem2_source *cfg,
	char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#endif

int
pmem2_source_device_usc(const struct pmem2_source *cfg, uint64_t *usc)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_iterator_new(const struct pmem2_source *cfg,
		struct pmem2_badblock_iterator **pbb)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_next(struct pmem2_badblock_iterator *pbb,
		struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

void pmem2_badblock_iterator_delete(
		struct pmem2_badblock_iterator **pbb)
{
}

int
pmem2_badblock_clear(const struct pmem2_source *cfg,
		const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}
