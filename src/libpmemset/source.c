// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * source.c -- implementation of common config API
 */

#include "libpmemset.h"
#include "pmemset_utils.h"
#include "source.h"

struct pmemset_source {
	enum pmemset_source_type type;
	union {
		struct pmem2_source *pmem2_src;
	} value;
};

/*
 * pmemset_source_from_pmem2 -- create pmemset source using source from pmem2
 */
int
pmemset_source_from_pmem2(struct pmemset_source **src,
		struct pmem2_source *pmem2_src)
{
	PMEMSET_ERR_CLR();

	*src = NULL;

	if (!pmem2_src) {
		ERR("pmem2_source cannot be NULL");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_PMEM2;
	srcp->value.pmem2_src = pmem2_src;

	*src = srcp;

	return 0;
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
