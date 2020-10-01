// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * source.c -- implementation of common config API
 */

#include "source.h"
#include "libpmemset.h"

/*
 * pmemset_source_from_external -- not supported
 */
int
pmemset_source_from_external(struct pmemset_source **src,
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
 * pmemset_source_from_file_paramsU -- not supported
 */
int
pmemset_source_from_file_paramsU(struct pmemset_source **src,
		const wchar_t *file)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_from_file_paramsW -- not supported
 */
int
pmemset_source_from_file_paramsW(struct pmemset_source **src,
		const wchar_t *file)
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
 * pmemset_source_fallocate -- not supported
 */
int
pmemset_source_fallocate(struct pmemset_source *src, int flags, size_t offset,
		size_t size)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_delete -- not supported
 */
int
pmemset_source_delete(struct pmemset_source **src)
{
	return PMEMSET_E_NOSUPP;
}
