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
 * util_windows.c -- misc utilities with OS-specific implementation
 */

#include <string.h>
#include <tchar.h>
#include "util.h"
#include "out.h"
#include "file.h"

/* Windows CRT doesn't support all errors, add unmapped here */
#define ENOTSUP_STR "Operation not supported"
#define ECANCELED_STR "Operation canceled"
#define ENOERROR 0
#define ENOERROR_STR "Success"
#define UNMAPPED_STR "Unmapped error"

/*
 * util_strerror -- return string describing error number
 *
 * XXX: There are many other POSIX error codes that are not recognized by
 * strerror_s(), so eventually we may want to implement this in a similar
 * fashion as strsignal().
 */
void
util_strerror(int errnum, char *buff, size_t bufflen)
{
	switch (errnum) {
	case ENOERROR:
		strcpy_s(buff, bufflen, ENOERROR_STR);
		break;
	case ENOTSUP:
		strcpy_s(buff, bufflen, ENOTSUP_STR);
		break;
	case ECANCELED:
		strcpy_s(buff, bufflen, ECANCELED_STR);
		break;
	default:
		if (strerror_s(buff, bufflen, errnum))
			strcpy_s(buff, bufflen, UNMAPPED_STR);
	}
}

/*
 * util_realpath -- get canonicalized absolute pathname
 */
char *
util_realpath(const char *path)
{
	ASSERT(util_is_absolute_path(path));
	return strdup(path);
}

/*
 * util_compare_file_inodes -- compare device and inodes of two files
 */
int
util_compare_file_inodes(const char *path1, const char *path2)
{
	return strcmp(path1, path2) != 0;
}

/*
 * util_aligned_malloc -- allocate aligned memory
 */
void *
util_aligned_malloc(size_t alignment, size_t size)
{
	return _aligned_malloc(size, alignment);
}

/*
 * util_aligned_free -- free allocated memory in util_aligned_malloc
 */
void
util_aligned_free(void *ptr)
{
	_aligned_free(ptr);
}
