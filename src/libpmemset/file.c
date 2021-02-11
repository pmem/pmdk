// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * file.c -- implementation of common file API
 */

#include <libpmem2.h>

#include "alloc.h"
#include "file.h"
#include "pmemset_utils.h"

struct pmemset_file {
	bool close;
	struct pmem2_source *pmem2_src;
#ifdef _WIN32
		HANDLE handle;
#else
		int fd;
#endif
};

/*
 * pmemset_file_get_pmem2_source -- retrieves the pmem2_source from pmemset_file
 */
struct pmem2_source *
pmemset_file_get_pmem2_source(struct pmemset_file *file)
{
	return file->pmem2_src;
}

/*
 * pmemset_file_init -- initializes pmemset_file structure based on the values
 *                      stored in pmem2_source
 */
static int
pmemset_file_init(struct pmemset_file *file, struct pmem2_source *pmem2_src)
{
	int ret;
#ifdef _WIN32
	HANDLE handle;
	ret = pmem2_source_get_handle(pmem2_src, &handle);
	file->handle = handle;
#else
	int fd;
	ret = pmem2_source_get_fd(pmem2_src, &fd);
	file->fd = fd;
#endif
	if (ret)
		return ret;

	file->pmem2_src = pmem2_src;

	return 0;
}

/*
 * pmemset_file_from_file -- allocates pmemset_file structure and initializes it
 *                           based on the provided path to the file
 */
int
pmemset_file_from_file(struct pmemset_file **file, char *path,
		unsigned flags)
{
	*file = NULL;

	int ret;
	struct pmemset_file *f = pmemset_malloc(sizeof(*f), &ret);
	if (ret)
		return PMEMSET_E_ERRNO;

	struct pmem2_source *pmem2_src;
	ret = pmemset_file_create_pmem2_src(&pmem2_src, path, flags);
	if (ret)
		goto err_free_file;

	ret = pmemset_file_init(f, pmem2_src);
	if (ret)
		goto err_delete_pmem2_src;

	f->close = true;

	*file = f;

	return 0;

err_delete_pmem2_src:
	pmemset_file_dispose_pmem2_src(&pmem2_src);
err_free_file:
	Free(f);
	return ret;
}

/*
 * pmemset_file_from_pmem2 -- allocates pmemset_file structure and initializes
 *                            it based on the provided pmem2_source
 */
int
pmemset_file_from_pmem2(struct pmemset_file **file,
		struct pmem2_source *pmem2_src)
{
	*file = NULL;

	if (!pmem2_src) {
		ERR("invalid pmem2 source provided");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	int ret;
	struct pmemset_file *f = pmemset_malloc(sizeof(*f), &ret);
	if (ret)
		return PMEMSET_E_ERRNO;

	ret = pmemset_file_init(f, pmem2_src);
	if (ret)
		goto err_free_file;

	f->close = false;

	*file = f;

	return 0;

err_free_file:
	Free(f);
	return ret;
}

/*
 * pmemset_file_delete -- deletes and closes the structure describing the file
 */
void
pmemset_file_delete(struct pmemset_file **file)
{
	struct pmemset_file *f = *file;

	if (f->close) {
		pmemset_file_close(f->pmem2_src);
		pmem2_source_delete(&f->pmem2_src);
	}

	Free(f);
	*file = NULL;
}
