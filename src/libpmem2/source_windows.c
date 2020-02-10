/*
 * Copyright 2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

/*
 * Copyright 2019-2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * source_windows.c -- windows specific pmem2_config implementation
 */

#include <Windows.h>
#include "config.h"
#include "libpmem2.h"
#include "config.h"
#include "out.h"
#include "pmem2_utils.h"
#include "util.h"

/*
 * pmem2_source_from_fd -- create a new data source instance
 */
int
pmem2_source_from_fd(struct pmem2_source **src, int fd)
{
	if (fd < 0) {
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
	*src = NULL;
	int ret;

	if (handle == INVALID_HANDLE_VALUE) {
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	BY_HANDLE_FILE_INFORMATION file_info;
	int ret = pmem2_win_stat(handle, &file_info);
	if (ret) {
		return ret;
	}

	/* XXX: winapi doesn't provide option to get open flags from HANDLE */

	struct pmem2_source *srcp = pmem2_malloc(sizeof(**src), &ret);

	if (ret)
		return ret;

	ASSERTne(srcp, NULL);

	srcp->handle = handle;
	*src = srcp;

	return 0;
}

/*
 * pmem2_source_file_size -- get a file size of the file handle stored in
 * the provided source
 */
int
pmem2_source_file_size(const struct pmem2_source *src, size_t *size)
{
	LOG(3, "handle %p", src->handle);

	BY_HANDLE_FILE_INFORMATION info;
	int ret = pmem2_win_stat(src->handle, &info);
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
pmem2_source_alignment(const struct pmem2_source *cfg, size_t *alignment)
{
	LOG(3, "handle %p", src->handle);

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
