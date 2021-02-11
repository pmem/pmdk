// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

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
 * pmemset_file_create_pmem2_src -- create pmem2_source structure based on the
 *                                  provided path to the file
 */
int
pmemset_file_create_pmem2_src(struct pmem2_source **pmem2_src, char *path,
		unsigned flags)
{
	/* config doesn't have information about open parameters for now */
	DWORD access = GENERIC_READ | GENERIC_WRITE;

	/* Init file create disposition flags */
	DWORD disposition = OPEN_EXISTING;

	/* Check create disposition flags */
	if (flags & PMEMSET_SOURCE_FILE_CREATE_ALWAYS)
		disposition = CREATE_ALWAYS;
	else if (flags & PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED)
		disposition = OPEN_ALWAYS;

	HANDLE handle = CreateFile(path, access, 0, NULL, disposition,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		ERR("!CreateFile %s", path);
		return pmemset_lasterror_to_err();
	}

	int ret = pmem2_source_from_handle(pmem2_src, handle);
	if (ret)
		goto err_close_file;

	return 0;

err_close_file:
	CloseHandle(handle);
	return ret;
}

/*
 * pmemset_file_close -- close the file described by the file handle
 */
int
pmemset_file_close(struct pmem2_source *pmem2_src)
{
	HANDLE handle;
	int ret = pmem2_source_get_handle(pmem2_src, &handle);
	if (ret)
		return ret;

	ret =  CloseHandle(handle);
	if (ret == 0) {
		ERR("!CloseHandle");
		return PMEMSET_E_ERRNO;
	}

	return 0;
}

/*
 * pmemset_file_dispose_pmem2_src -- disposes of the pmem2_source structure
 */
int
pmemset_file_dispose_pmem2_src(struct pmem2_source **pmem2_src)
{
	int ret = pmemset_file_close(*pmem2_src);
	if (ret)
		return ret;

	return pmem2_source_delete(pmem2_src);
}
