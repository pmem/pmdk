// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * source.c -- implementation of common config API
 */

#include "libpmemset.h"

#include "source.h"

/*
 * pmemset_source_from_pmem2 -- not supported
 */
int
pmemset_source_from_pmem2(struct pmemset_source **src,
		struct pmem2_source *ext_source)
{
	return PMEMSET_E_NOSUPP;
}

#ifndef _WIN32
/*
 * pmemset_source_from_file -- not supported
 */
int
pmemset_source_from_file(struct pmemset_source **src, const char *file)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_from_temporary -- not supported
 */
int
pmemset_source_from_temporary(struct pmemset_source **src, const char *dir)
{
	return PMEMSET_E_NOSUPP;
}
#else
/*
 * pmemset_source_from_fileU -- not supported
 */
int
pmemset_source_from_fileU(struct pmemset_source **src, const char *file)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_from_fileW -- not supported
 */
pmemset_source_from_fileW(struct pmemset_source **src, const wchar_t *file)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_from_temporaryU -- not supported
 */
int
pmemset_source_from_temporaryU(struct pmemset_source **src, const char *dir)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_from_temporaryW -- not supported
 */
int
pmemset_source_from_temporaryW(struct pmemset_source **src, const wchar_t *dir)
{
	return PMEMSET_E_NOSUPP;
}
#endif

/*
 * pmemset_source_delete -- not supported
 */
int
pmemset_source_delete(struct pmemset_source **src)
{
	return PMEMSET_E_NOSUPP;
}
