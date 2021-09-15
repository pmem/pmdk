// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * file_windows.c -- implementation of file API (windows)
 */

#include <libpmem2.h>
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include "alloc.h"
#include "file.h"
#include "libpmemset.h"
#include "pmemset_utils.h"

/*
 * generate_file_from_template -- generate a unique temporary
 * filename from template
 */
static HANDLE
generate_file_from_template(char *temp)
{
	unsigned rnd;
	HANDLE h = NULL;

	char *path = _mktemp(temp);
	if (path == NULL)
		return h;

	int ret;
	char *npath = pmemset_malloc(sizeof(*npath) * strlen(path) + _MAX_FNAME,
		&ret);
	if (ret)
		return h;

	strcpy(npath, path);

	/*
	 * Use rand_s to generate more unique tmp file name than _mktemp do.
	 * In case with multiple threads and multiple files even after close()
	 * file name conflicts occurred.
	 * It resolved issue with synchronous removing
	 * multiples files by system.
	 */
	rand_s(&rnd);

	ret = _snprintf(npath + strlen(npath), _MAX_FNAME, "%u", rnd);
	if (ret < 0)
		goto out;

	DWORD access = GENERIC_READ | GENERIC_WRITE;
	h = CreateFile(npath, access, 0, NULL, CREATE_NEW,
		FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		ERR("!CreateFile %s", npath);
		h = NULL;
		goto out;
	}

out:
	Free(npath);
	return h;

}

/*
 * create_tmpfile -- create a temporary file in dir
 */
static HANDLE
create_tmpfile(const char *dir, const char *templ)
{
	LOG(3, "dir \"%s\" template \"%s\"", dir, templ);

	HANDLE h = NULL;
	size_t len = strlen(dir) + strlen(templ) + 1;
	int ret;
	char *fullname = pmemset_malloc(sizeof(*fullname) * len, &ret);
	if (ret)
		return h;

	ret = _snprintf(fullname, len, "%s%s", dir, templ);
	if (ret < 0 || ret >= len) {
		ERR("snprintf: %d", ret);
		goto err;
	}

	LOG(4, "fullname \"%s\"", fullname);

	h = generate_file_from_template(fullname);
	if (h == NULL) {
		ERR("cannot create temporary file");
		goto err;
	}

	return h;

err:
	Free(fullname);
	return h;
}

/*
 * pmemset_file_create_pmem2_src -- create pmem2_source structure based on the
 *                                  provided path to the file
 */
int
pmemset_file_create_pmem2_src(struct pmem2_source **pmem2_src, char *path,
		uint64_t flags)
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
 * pmemset_file_create_pmem2_src_from_temp -- create pmem2_source
 *		structure based on the provided dir to temp file
 */
int
pmemset_file_create_pmem2_src_from_temp(struct pmem2_source **pmem2_src,
		char *dir)
{
	HANDLE h = create_tmpfile(dir, OS_DIR_SEP_STR"pmemsetXXXXXX");

	if (!h) {
		ERR("failed to create temporary file at \"%s\"", dir);
		return PMEMSET_E_CANNOT_CREATE_TEMP_FILE;
	}

	int ret = pmem2_source_from_handle(pmem2_src, h);
	if (ret)
		goto err_close_file;

	return 0;

err_close_file:
	CloseHandle(h);
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

/*
 * pmemset_file_grow -- grow file from pmemset_file to a specified len
 */
int
pmemset_file_grow(struct pmemset_file *file, size_t len)
{
	HANDLE h = pmemset_file_get_handle(file);

	LARGE_INTEGER distanceToMove = {0};
	distanceToMove.QuadPart = (os_off_t)len;

	if (!SetFilePointerEx(h, distanceToMove, NULL, FILE_BEGIN)) {
		ERR("!SetFilePointer");
		return pmemset_lasterror_to_err();
	}

	if (!SetEndOfFile(h)) {
		ERR("!SetEndOfFile");
		return pmemset_lasterror_to_err();
	}

	return 0;
}
