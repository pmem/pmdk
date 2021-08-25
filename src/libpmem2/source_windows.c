// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2021, Intel Corporation */

/*
 * source_windows.c -- windows specific pmem2_source implementation
 */

#include <Windows.h>
#include "config.h"
#include "libpmem2.h"
#include "config.h"
#include "os.h"
#include "out.h"
#include "pmem2_utils.h"
#include "source.h"
#include "util.h"

/*
 * pmem2_source_from_fd -- create a new data source instance
 */
int
pmem2_source_from_fd(struct pmem2_source **src, int fd)
{
	PMEM2_ERR_CLR();

	*src = NULL;

	if (fd < 0) {
		ERR("Invalid file descriptor value %d", fd);
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	HANDLE handle = (HANDLE)_get_osfhandle(fd);

	if (handle == INVALID_HANDLE_VALUE) {
		/*
		 * _get_osfhandle aborts in an error case, so technically
		 * this is dead code. But according to MSDN it is
		 * setting an errno on failure, so we can return it in case of
		 * "windows magic" happen and this function "accidentally"
		 * will not abort.
		 */
		ERR("!_get_osfhandle");
		if (errno == EBADF)
			return PMEM2_E_INVALID_FILE_HANDLE;
		return PMEM2_E_ERRNO;
	}

	return pmem2_source_from_handle(src, handle);
}

/*
 * pmem2_win_stat -- retrieve information about handle
 */
static int
pmem2_win_stat(HANDLE handle, BY_HANDLE_FILE_INFORMATION *info)
{
	if (!GetFileInformationByHandle(handle, info)) {
		ERR("!!GetFileInformationByHandle");
		if (GetLastError() == ERROR_INVALID_HANDLE)
			return PMEM2_E_INVALID_FILE_HANDLE;
		else
			return pmem2_lasterror_to_err();
	}

	if (info->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		ERR(
			"using directory doesn't make any sense in context of pmem2");
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	return 0;
}

/*
 * pmem2_source_from_fd -- create a new data source instance
 */
int
pmem2_source_from_handle(struct pmem2_source **src, HANDLE handle)
{
	PMEM2_ERR_CLR();

	*src = NULL;
	int ret;

	if (handle == INVALID_HANDLE_VALUE) {
		ERR("Invalid file handle has been passed");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	BY_HANDLE_FILE_INFORMATION file_info;
	ret = pmem2_win_stat(handle, &file_info);
	if (ret)
		return ret;

	enum pmem2_file_type ftype;
	DWORD fileAttrbs = file_info.dwFileAttributes;
	if ((file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		ERR("cannot set fd/handle to directory in pmem2_source_from_fd"
				" or in pmem2_source_from_handle");
		return PMEM2_E_INVALID_FILE_TYPE;
	} else {
		ftype = PMEM2_FTYPE_REG;
	}

	/* XXX: winapi doesn't provide option to get open flags from HANDLE */

	struct pmem2_source *srcp = pmem2_malloc(sizeof(**src), &ret);

	if (ret)
		return ret;

	ASSERTne(srcp, NULL);

	srcp->type = PMEM2_SOURCE_HANDLE;
	srcp->value.ftype = ftype;
	srcp->value.handle = handle;
	*src = srcp;

	return 0;
}

/*
 * pmem2_source_size -- get a size of the file handle stored in the provided
 * source
 */
int
pmem2_source_size(const struct pmem2_source *src, size_t *size)
{
	LOG(3, "type %d", src->type);
	PMEM2_ERR_CLR();

	int ret;

	if (src->type == PMEM2_SOURCE_ANON) {
		*size = src->value.size;
		return 0;
	}
	ASSERTeq(src->type, PMEM2_SOURCE_HANDLE);

	BY_HANDLE_FILE_INFORMATION info;
	ret = pmem2_win_stat(src->value.handle, &info);
	if (ret)
		return ret;

	*size = ((size_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;

	LOG(4, "file length %zu", *size);

	return 0;
}

/*
 * pmem2_source_alignment -- get alignment from the system info
 */
int
pmem2_source_alignment(const struct pmem2_source *src, size_t *alignment)
{
	LOG(3, "type %d", src->type);
	PMEM2_ERR_CLR();

	SYSTEM_INFO info;
	GetSystemInfo(&info);

	*alignment = (size_t)info.dwAllocationGranularity;

	if (!util_is_pow2(*alignment)) {
		ERR("alignment (%zu) has to be a power of two", *alignment);
		return PMEM2_E_INVALID_ALIGNMENT_VALUE;
	}

	LOG(4, "alignment %zu", *alignment);

	return 0;
}

/*
 * pmem2_source_get_handle -- get file handler from provided source
 */
int
pmem2_source_get_handle(const struct pmem2_source *src, HANDLE *h)
{
	LOG(3, "src type %d", src->type);
	PMEM2_ERR_CLR();

	if (src->type == PMEM2_SOURCE_HANDLE) {
		*h = src->value.handle;
	} else {
		ERR(
			"File handle is not set, source type does not support file handles");
		return PMEM2_E_FILE_HANDLE_NOT_SET;
	}

	return 0;
}
