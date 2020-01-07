// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

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

/*
 * sysconf -- get configuration information at run time
 */
static __inline long
sysconf(int p)
{
	SYSTEM_INFO si;
	int ret = 0;

	switch (p) {
	case _SC_PAGESIZE:
		GetSystemInfo(&si);
		return si.dwPageSize;

	case _SC_NPROCESSORS_ONLN:
		for (int i = 0; i < GetActiveProcessorGroupCount(); i++) {
			ret += GetActiveProcessorCount(i);
		}
		return ret;

	default:
		return 0;
	}

}

#define getpid _getpid

/*
 * pread -- read from a file descriptor at given offset
 */
static ssize_t
pread(int fd, void *buf, size_t count, os_off_t offset)
{
	__int64 position = _lseeki64(fd, 0, SEEK_CUR);
	_lseeki64(fd, offset, SEEK_SET);
	int ret = _read(fd, buf, (unsigned)count);
	_lseeki64(fd, position, SEEK_SET);
	return ret;
}

/*
 * pwrite -- write to a file descriptor at given offset
 */
static ssize_t
pwrite(int fd, const void *buf, size_t count, os_off_t offset)
{
	__int64 position = _lseeki64(fd, 0, SEEK_CUR);
	_lseeki64(fd, offset, SEEK_SET);
	int ret = _write(fd, buf, (unsigned)count);
	_lseeki64(fd, position, SEEK_SET);
	return ret;
}

#define S_ISBLK(x) 0 /* BLK devices not exist on Windows */

/*
 * basename -- parse pathname and return filename component
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
 * dirname -- parse pathname and return directory component
 */
static char *
dirname(char *path)
{
	if (path == NULL)
		return ".";

	size_t len = strlen(path);
	if (len == 0)
		return ".";

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

#endif /* UNISTD_H */
