// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * file_posix.c -- implementation of file API (posix)
 */

#include <fcntl.h>
#include <libpmem2.h>
#include <stdbool.h>

#include "file.h"
#include "libpmemset.h"
#include "pmemset_utils.h"

/*
 * pmemset_file_from_file -- create pmemset_file structure based on the provided
 *                           path to the file
 */
int
pmemset_file_from_file(struct pmemset_file **file, char *path,
		struct pmemset_config *cfg)
{
	struct pmem2_source *pmem2_src;

	*file = NULL;

	/* config doesn't have information about open parameters for now */
	int access = O_RDWR;
	int fd = os_open(path, access);
	if (fd < 0) {
		ERR("!open %s", path);
		return PMEMSET_E_ERRNO;
	}

	int ret = pmem2_source_from_fd(&pmem2_src, fd);
	if (ret)
		goto err_close_file;

	ret = pmemset_file_from_pmem2(file, pmem2_src);
	if (ret)
		goto err_delete_pmem2_src;

	(*file)->close = true;

	return 0;

err_delete_pmem2_src:
	pmem2_source_delete(&pmem2_src);
err_close_file:
	os_close(fd);
	return ret;
}

/*
 * pmemset_file_from_pmem2 -- create pmemset_file structure based on the
 *                            provided pmem2_source structure
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

	int fd;
	int ret = pmem2_source_get_fd(pmem2_src, &fd);
	if (ret) {
		ERR("invalid pmem2 source provided");
		return PMEMSET_E_ERRNO;
	}

	struct pmemset_file *f = pmemset_malloc(sizeof(*f), &ret);
	if (ret)
		return PMEMSET_E_ERRNO;

	f->pmem2.src = pmem2_src;
	f->fd = fd;
	f->close = false;

	*file = f;

	return 0;
}

/*
 * pmemset_file_close -- close the file described in pmemset_file
 */
int
pmemset_file_close(struct pmemset_file *file)
{
	int ret = os_close(file->fd);
	if (ret) {
		ERR("!close");
		return PMEMSET_E_ERRNO;
	}

	return 0;
}
