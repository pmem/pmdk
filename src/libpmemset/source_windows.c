// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * source_posix.c -- implementation of config API (posix)
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "libpmemset.h"

#include "os.h"
#include "pmemset_utils.h"
#include "source.h"

/*
 * pmemset_source_from_fileU -- initializes source structure and stores a path
 *                              to the file
 */
int
pmemset_source_from_fileU(struct pmemset_source **src, const char *file)
{
	LOG(3, "src %p file %s", src, file);
	PMEMSET_ERR_CLR();

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->filepath = strdup(file);

	*src = srcp;

	return 0;
}

/*
 * pmemset_source_from_fileW -- initializes source structure and stores a path
 *                              to the file
 */
int
pmemset_source_from_fileW(struct pmemset_source **src, const wchar_t *file)
{
	LOG(3, "src %p file %s", src, file);
	PMEMSET_ERR_CLR();

	const char *ufile = util_toUTF8(file);

	return pmemset_source_from_fileU(src, ufile);
}
