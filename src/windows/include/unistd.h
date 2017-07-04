/*
 * Copyright 2015-2017, Intel Corporation
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
 * unistd.h -- compatibility layer for POSIX operating system API
 */

#ifndef UNISTD_H
#define UNISTD_H 1

#include <stdio.h>

#define _SC_PAGESIZE 0
#define _SC_NPROCESSORS_ONLN 1

#define R_OK 04
#define W_OK 02
#define X_OK 00 /* execute permission doesn't exist on Windows */
#define F_OK 00

static __inline long
sysconf(int p)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	switch (p) {
	case _SC_PAGESIZE:
		return si.dwPageSize;
		break;

	case _SC_NPROCESSORS_ONLN:
		return si.dwNumberOfProcessors;
		break;

	default:
		return 0;
	}

}

#define getpid _getpid

/*
 * pread - windows port of pread function
 */
static ssize_t
pread(int fd, void *buf, size_t count, off_t offset)
{
	__int64 position = _lseeki64(fd, 0, SEEK_CUR);
	_lseeki64(fd, offset, SEEK_SET);
	int ret = _read(fd, buf, (unsigned)count);
	_lseeki64(fd, position, SEEK_SET);
	return ret;
}

/*
 * pwrite - windows port of pwrite function
 */
static ssize_t
pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	__int64 position = _lseeki64(fd, 0, SEEK_CUR);
	_lseeki64(fd, offset, SEEK_SET);
	int ret = _write(fd, buf, (unsigned)count);
	_lseeki64(fd, position, SEEK_SET);
	return ret;
}

#define S_ISBLK(x) 0 /* BLK devices not exist on Windows */

/*
 * basename - windows implementation of basename function
 */
static char *
basename(char *path)
{
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];
	_splitpath(path, NULL, NULL, fname, ext);

	sprintf(path, "%s%s", fname, ext);

	return path;
}

/*
 * dirname - windows implementation of dirname function
 */
static char *
dirname(char *path)
{
	size_t len = strlen(path);

	if (len == 0)
		return NULL;

	char *end = path + len;

	/* strip trailing forslashes and backslashes */
	while ((--end) > path) {
		if (*end != '\\' && *end != '/') {
			*(end + 1) = '\0';
			break;
		}
	}

	/* strip basename */
	while ((--end) > path) {
		if (*end == '\\' || *end == '/') {
			*end = '\0';
			break;
		}
	}

	if (end != path) {
		return path;
		/* handle edge cases */
	} else if (*end == '\\' || *end == '/') {
		*(end + 1) = '\0';
	} else {
		*end++ = '.';
		*end = '\0';
	}

	return path;
}

int ftruncate(int fd, off_t length);

#endif /* UNISTD_H */
