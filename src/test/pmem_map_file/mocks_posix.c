// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * mocks_posix.c -- mocked functions used in pmem_map_file.c (Posix-specific)
 */

#define _GNU_SOURCE
#include "unittest.h"
#include <dlfcn.h>

#define MAX_LEN (4 * 1024 * 1024)

/*
 * posix_fallocate -- interpose on libc posix_fallocate()
 */
int
posix_fallocate(int fd, os_off_t offset, off_t len)
{
	UT_OUT("posix_fallocate: off %ju len %ju", offset, len);

	static int (*posix_fallocate_ptr)(int fd, os_off_t offset, off_t len);

	if (posix_fallocate_ptr == NULL)
		posix_fallocate_ptr = dlsym(RTLD_NEXT, "posix_fallocate");

	if (len > MAX_LEN)
		return ENOSPC;

	return (*posix_fallocate_ptr)(fd, offset, len);
}

/*
 * ftruncate -- interpose on libc ftruncate()
 */
int
ftruncate(int fd, os_off_t len)
{
	UT_OUT("ftruncate: len %ju", len);

	static int (*ftruncate_ptr)(int fd, os_off_t len);

	if (ftruncate_ptr == NULL)
		ftruncate_ptr = dlsym(RTLD_NEXT, "ftruncate");

	if (len > MAX_LEN) {
		errno = ENOSPC;
		return -1;
	}

	return (*ftruncate_ptr)(fd, len);
}
