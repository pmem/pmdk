// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * file_windows.c -- implementation of file API (windows)
 */

#include <libpmem2.h>
#include <windows.h>
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
	DWORD access = GENERIC_READ | GENERIC_WRITE;
	HANDLE handle = CreateFile(path, access, 0, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		ERR("!CreateFile %s", path);
		return pmemset_lasterror_to_err();
	}

	int ret = pmem2_source_from_handle(&pmem2_src, handle);
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
	CloseHandle(handle);
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

	HANDLE handle;
	int ret = pmem2_source_get_handle(pmem2_src, &handle);
	if (ret) {
		ERR("invalid pmem2 source provided");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	struct pmemset_file *f = pmemset_malloc(sizeof(*f), &ret);
	if (ret)
		return PMEMSET_E_ERRNO;

	f->pmem2.src = pmem2_src;
	f->handle = handle;
	f->close = false;

	*file = f;

	return 0;
}

/*
 * pmemset_file_close -- create pmemset_file structure based on the provided
 *                       pmem2_source structure
 */
int
pmemset_file_close(struct pmemset_file *file)
{
	int ret =  CloseHandle(file->handle);
	if (ret == 0) {
		ERR("!CloseHandle");
		return PMEMSET_E_ERRNO;
	}

	return 0;
}
