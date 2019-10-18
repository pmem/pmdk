/*
 * Copyright 2016-2019, Intel Corporation
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
 * mocks_windows.c -- mocked functions used in util_poolset.c
 */

#include "pmem.h"
#include "util.h"
#include "unittest.h"

extern const char *Open_path;
extern os_off_t Fallocate_len;
extern size_t Is_pmem_len;

/*
 * os_open -- open mock
 *
 * due to diffrences in function mocking on linux we are wraping os_open
 * but on linux we just wrap open syscall
 */
FUNC_MOCK(os_open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	if (strcmp(Open_path, path) == 0) {
		UT_OUT("mocked open: %s", path);
		errno = EACCES;
		return -1;
	}

	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	return _FUNC_REAL(os_open)(path, flags, mode);
}
FUNC_MOCK_END

/*
 * posix_fallocate -- posix_fallocate mock
 */
FUNC_MOCK(os_posix_fallocate, int, int fd, os_off_t offset, os_off_t len)
FUNC_MOCK_RUN_DEFAULT {
	if (Fallocate_len == len) {
		UT_OUT("mocked fallocate: %ju", len);
		return ENOSPC;
	}
	return _FUNC_REAL(os_posix_fallocate)(fd, offset, len);
}
FUNC_MOCK_END

/*
 * pmem_is_pmem -- pmem_is_pmem mock
 */
FUNC_MOCK(pmem_is_pmem, int, const void *addr, size_t len)
FUNC_MOCK_RUN_DEFAULT {
	if (Is_pmem_len == len) {
		UT_OUT("mocked pmem_is_pmem: %zu", len);
		return 1;
	}
	return _FUNC_REAL(pmem_is_pmem)(addr, len);
}
FUNC_MOCK_END

/*
 * On Windows libpmem is statically linked to util_poolset test, but we
 * don't want its ctor to initialize 'out' module.
 */

/*
 * libpmem_init -- load-time initialization for libpmem
 *
 * Called automatically by the run-time loader.
 */
CONSTRUCTOR(libpmem_init)
void
libpmem_init(void)
{
	pmem_init();
}
