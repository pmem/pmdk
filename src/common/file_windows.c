/*
 * Copyright 2015-2016, Intel Corporation
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
 * file_windows.c -- Windows emulation of Linux-specific system calls
 */

/*
 * XXX - The initial approach to NVML for Windows port was to minimize the
 * amount of changes required in the core part of the library, and to avoid
 * preprocessor conditionals, if possible.  For that reason, some of the
 * Linux system calls that have no equivalents on Windows have been emulated
 * using Windows API.
 * Note that it was not a goal to fully emulate POSIX-compliant behavior
 * of mentioned functions.  They are used only internally, so current
 * implementation is just good enough to satisfy NVML needs and to make it
 * work on Windows.
 *
 * This is a subject for change in the future.  Likely, all these functions
 * will be replaced with "util_xxx" wrappers with OS-specific implementation
 * for Linux and Windows.
 */

#include <windows.h>
#include <sys/stat.h>
#include <sys/file.h>

/*
 * mkstemp -- generate a unique temporary filename from template
 */
int
mkstemp(char *temp)
{
	/* XXX - limited number of unique file names */
	char *path = _mktemp(temp);
	if (path == NULL)
		return -1;

	return open(path, O_RDWR | O_CREAT | O_EXCL, S_IWRITE | S_IREAD);
}

/*
 * posix_fallocate -- allocate file space
 */
int
posix_fallocate(int fd, off_t offset, off_t size)
{
	if (offset > 0)
		size += offset;

	off_t len = _filelengthi64(fd);
	if (len < 0)
		return -1;

	if (size < len)
		return 0;

	return _chsize_s(fd, size);
}

/*
 * flock -- apply or remove an advisory lock on an open file
 */
int
flock(int fd, int operation)
{
	int flags = 0;
	SYSTEM_INFO  systemInfo;

	GetSystemInfo(&systemInfo);

	switch (operation & (LOCK_EX | LOCK_SH | LOCK_UN)) {
		case LOCK_EX:
		case LOCK_SH:
			if (operation & LOCK_NB)
				flags = _LK_NBLCK;
			else
				flags = _LK_LOCK;
			break;

		case LOCK_UN:
			flags = _LK_UNLCK;
			break;

		default:
			errno = EINVAL;
			return -1;
	}

	off_t filelen = _filelengthi64(fd);
	if (filelen < 0)
		return -1;

	/* for our purpose it's enough to lock the first page of the file */
	long len = (filelen > systemInfo.dwPageSize) ?
				systemInfo.dwPageSize : (long)filelen;
	return _locking(fd, flags, len);
}

/*
 * ftruncate -- truncate a file to a specified length
 */
int
ftruncate(int fd, off_t length)
{
	return _chsize_s(fd, length);
}
