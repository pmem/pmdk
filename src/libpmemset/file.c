// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

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

void
pmemset_file_delete(struct pmemset_file **file)
{
	struct pmemset_file *f = *file;

	if (f->close) {
#ifdef _WIN32
		pmemset_file_close(f->handle);
#else
		pmemset_file_close(f->fd);
#endif
		pmem2_source_delete(&f->pmem2_src);
	}

	Free(f);
	*file = NULL;
}

/*
 * pmemset_file_init -- initializes pmemset_file structure
 */
static int
pmemset_file_init(struct pmemset_file **file, struct pmem2_source *pmem2_src)
{
	int ret;
	struct pmemset_file *f = pmemset_malloc(sizeof(*f), &ret);
	if (ret)
		return PMEMSET_E_ERRNO;

#ifdef _WIN32
	HANDLE handle;
	ret = pmem2_source_get_handle(pmem2_src, &handle);
	f->handle = handle;
#else
	int fd;
	ret = pmem2_source_get_fd(pmem2_src, &fd);
	f->fd = fd;
#endif
	if (ret)
		goto err_free_file;

	f->pmem2_src = pmem2_src;

	*file = f;

	return 0;

err_free_file:
	Free(f);
	return ret;
}

/*
 * pmemset_file_from_file -- allocates pmemset_file structure and initializes it
 *                           based on the provided path to the file
 */
int
pmemset_file_from_file(struct pmemset_file **file, char *path,
		struct pmemset_config *cfg)
{
	*file = NULL;

	struct pmem2_source *pmem2_src;
	int ret = pmemset_file_create_pmem2_src(&pmem2_src, path, cfg);
	if (ret)
		return ret;

	ret = pmemset_file_init(file, pmem2_src);
	if (ret)
		goto err_delete_pmem2_src;

	(*file)->close = true;

	return 0;

err_delete_pmem2_src:
	pmemset_file_dispose_pmem2_src(&pmem2_src);
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

	int ret = pmemset_file_init(file, pmem2_src);
	if (ret)
		return ret;

	(*file)->close = false;

	return 0;
}
