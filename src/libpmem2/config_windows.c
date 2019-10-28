/*
 * Copyright 2019, Intel Corporation
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
 * config_windows.c -- windows specific pmem2_config implementation
 */

#include <Windows.h>
#include "config.h"
#include "libpmem2.h"
#include "config.h"
#include "out.h"
#include "pmem2_utils.h"
#include "util.h"

/*
 * pmem2_config_set_fd -- sets fd in config struct
 */
int
pmem2_config_set_fd(struct pmem2_config *cfg, int fd)
{
	if (fd < 0) {
		cfg->handle = INVALID_HANDLE_VALUE;
		return 0;
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
		return PMEM2_E_ERRNO;
	}

	return pmem2_config_set_handle(cfg, handle);
}

/*
 * pmem2_config_set_handle -- convert fd to handle
 */
int
pmem2_config_set_handle(struct pmem2_config *cfg, HANDLE handle)
{
	if (handle == INVALID_HANDLE_VALUE) {
		cfg->handle = INVALID_HANDLE_VALUE;
		return 0;
	}

	BY_HANDLE_FILE_INFORMATION not_used;
	if (!GetFileInformationByHandle(handle, &not_used)) {
		ERR("!!GetFileInformationByHandle");
		return pmem2_lasterror_to_err();
	}
	/* XXX: winapi doesn't provide option to get open flags from HANDLE */
	cfg->handle = handle;
	return 0;
}

int
pmem2_config_get_file_size(struct pmem2_config *cfg, size_t *size)
{
	LOG(3, "handle %p", cfg->handle);

	if (cfg->handle == INVALID_HANDLE_VALUE) {
		ERR("cannot check size for invalid file handle");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	BY_HANDLE_FILE_INFORMATION info;
	if (!GetFileInformationByHandle(cfg->handle, &info)) {
		ERR("!!GetFileInformationByHandle");
		return pmem2_lasterror_to_err();
	}

	if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		ERR(
			"asking for size of a directory doesn't make any sense in context of pmem");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	*size = ((size_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;

	LOG(4, "file length %zu", *size);

	return 0;
}
