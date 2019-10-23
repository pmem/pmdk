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
#include "libpmem2.h"
#include "out.h"
#include "config.h"

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
		ERR("fd is not open file descriptor");
		return PMEM2_E_INVALID_ARGUMENT;
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
		ERR("HANDLE is invalid");
		return PMEM2_E_INVALID_ARGUMENT;
	}
	/* XXX: winapi doesn't provide option to get open flags from HANDLE */
	cfg->handle = handle;
	return 0;
}
