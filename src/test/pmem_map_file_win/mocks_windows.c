/*
 * Copyright 2014-2017, Intel Corporation
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
 * mocks_windows.c -- mocked functions used in pmem_map_file.c
 *                    (Windows-specific)
 */

#include "unittest.h"

#define MAX_LEN (4 * 1024 * 1024)

/*
 * posix_fallocate -- interpose on libc posix_fallocate()
 */
FUNC_MOCK(os_posix_fallocate, int, int fd, os_off_t offset, os_off_t len)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("posix_fallocate: off %ju len %ju", offset, len);
	if (len > MAX_LEN) {
		errno = ENOSPC;
		return -1;
	}
	return _FUNC_REAL(os_posix_fallocate)(fd, offset, len);
}
FUNC_MOCK_END

/*
 * ftruncate -- interpose on libc ftruncate()
 */
FUNC_MOCK(os_ftruncate, int, int fd, os_off_t len)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("ftruncate: len %ju", len);
	return _FUNC_REAL(os_ftruncate)(fd, len);
}
FUNC_MOCK_END
