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
 * util_linux.c -- misc utilities with OS-specific implementation
 */

#include <string.h>
#include <util.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "os.h"
#include "out.h"

/* pass through for Linux */
void
util_strerror(int errnum, char *buff, size_t bufflen)
{
	strerror_r(errnum, buff, bufflen);
}

/*
 * util_part_realpath -- get canonicalized absolute pathname
 *
 * As paths used in a poolset file have to be absolute (checked when parsing
 * a poolset file), here we only have to resolve symlinks.
 */
char *
util_part_realpath(const char *path)
{
	return realpath(path, NULL);
}

/*
 * util_compare_file_inodes -- compare device and inodes of two files;
 *                             this resolves hard links
 */
int
util_compare_file_inodes(const char *path1, const char *path2)
{
	struct stat sb1, sb2;
	if (os_stat(path1, &sb1)) {
		if (errno != ENOENT) {
			ERR("!stat failed for %s", path1);
			return -1;
		}
		LOG(1, "stat failed for %s", path1);
		errno = 0;
		return strcmp(path1, path2) != 0;
	}

	if (os_stat(path2, &sb2)) {
		if (errno != ENOENT) {
			ERR("!stat failed for %s", path2);
			return -1;
		}
		LOG(1, "stat failed for %s", path2);
		errno = 0;
		return strcmp(path1, path2) != 0;
	}

	return sb1.st_dev != sb2.st_dev || sb1.st_ino != sb2.st_ino;
}

/*
 * util_aligned_malloc -- allocate aligned memory
 */
void *
util_aligned_malloc(size_t alignment, size_t size)
{
	void *retval = NULL;

	errno = posix_memalign(&retval, alignment, size);

	return retval;
}

/*
 * util_aligned_free -- free allocated memory in util_aligned_malloc
 */
void
util_aligned_free(void *ptr)
{
	free(ptr);
}
