// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * file.c -- implementation of common file API
 */

#include <libpmem2.h>

#include "alloc.h"
#include "file.h"

/*
 * pmemset_file_get_pmem2_source -- retrieves the pmem2_source from pmemset_file
 */
struct pmem2_source *
pmemset_file_get_pmem2_source(struct pmemset_file *file)
{
	return file->pmem2.src;
}

void
pmemset_file_delete(struct pmemset_file **file)
{
	struct pmemset_file *f = *file;

	if (f->close) {
		pmemset_file_close(f);
		pmem2_source_delete(&f->pmem2.src);
	}

	Free(f);
	*file = NULL;
}
