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
#include <errno.h>
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
 * util_part_realpath -- get canonicalized absolute pathname for a part file
 *
 * On Windows, paths cannot be symlinks and paths used in a poolset have to
 * be absolute (checked when parsing a poolset file), so we just return
 * the path.
 */
char *
util_part_realpath(const char *path)
{
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

/*
 * util_toUTF8 -- allocating conversion from wide char string to UTF8
 */
char *
util_toUTF8(const wchar_t *wstr)
{
	int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1,
		NULL, 0, NULL, NULL);
	if (size == 0)
		goto err;

	char *str = Malloc(size * sizeof(char));
	if (str == NULL)
		goto out;

	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1, str,
		size, NULL, NULL) == 0) {
		Free(str);
		goto err;
	}

out:
	return str;

err:
	errno = EINVAL;
	return NULL;
}

/*
 * util_free_UTF8 -- free UTF8 string
 */
void util_free_UTF8(char *str) {
	Free(str);
}

/*
 * util_toUTF16 -- allocating conversion from UTF8 to wide char string
 */
wchar_t *
util_toUTF16(const char *str)
{
	int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1,
		NULL, 0);
	if (size == 0)
		goto err;

	wchar_t *wstr = Malloc(size * sizeof(wchar_t));
	if (wstr == NULL)
		goto out;

	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, wstr,
		size) == 0) {
		Free(wstr);
		goto err;
	}

out:
	return wstr;

err:
	errno = EINVAL;
	return NULL;
}

/*
 * util_free_UTF16 -- free wide char string
 */
void
util_free_UTF16(wchar_t *wstr)
{
	Free(wstr);
}

/*
 * util_toUTF16_buff -- non-allocating conversion from UTF8 to wide char string
 *
 * The user responsible for supplying a large enough out buffer.
 */
int
util_toUTF16_buff(const char *in, wchar_t *out, size_t out_size)
{
	ASSERT(out != NULL);

	int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in,
		-1, NULL, 0);
	if (size == 0 || out_size < size)
		goto err;

	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, in, -1,
		out, size) == 0)
		goto err;

	return 0;
err:
	errno = EINVAL;
	return -1;
}

/*
 * util_toUTF8_buff -- non-allocating conversion from wide char string to UTF8
 *
 * The user responsible for supplying a large enough out buffer.
 */
int
util_toUTF8_buff(const wchar_t *in, char *out, size_t out_size)
{
	ASSERT(out != NULL);

	int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in, -1,
		NULL, 0, NULL, NULL);
	if (size == 0 || out_size < size)
		goto err;

	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in, -1,
		out, size, NULL, NULL) == 0)
		goto err;

	return 0;
err:
	errno = EINVAL;
	return -1;
}

/*
 * util_getexecname -- return name of current executable
 */
char *
util_getexecname(char *path, size_t pathlen)
{
	ssize_t cc;
	if ((cc = GetModuleFileNameA(NULL, path, (DWORD)pathlen)) == 0)
		strcpy(path, "unknown");
	else
		path[cc] = '\0';

	return path;
}
