// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ras_none.c -- pmem2 ras function for non supported platform
 */

#include "libpmem2.h"

#ifndef _WIN32
int
pmem2_source_device_id(const struct pmem2_source *src, char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#else
int
pmem2_source_device_idW(const struct pmem2_source *src,
	wchar_t *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_device_idU(const struct pmem2_source *src, char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#endif

int
pmem2_source_device_usc(const struct pmem2_source *src, uint64_t *usc)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_iterator_new(const struct pmem2_source *src,
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
pmem2_badblock_clear(const struct pmem2_source *src,
		const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}
