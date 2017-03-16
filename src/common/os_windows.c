/*
 * Copyright 2017, Intel Corporation
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
 * os_windows.c -- windows abstraction layer
 */
#include "util.h"
#include "os.h"
#include <errno.h>

#define UTF8_BOM "\xEF\xBB\xBF"

/*
 * os_open -- open abstraction layer
 */
int
os_open(const char *pathname, int flags, ...)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret;

	if (flags & O_CREAT) {
		va_list arg;
		va_start(arg, flags);
		mode_t mode = va_arg(arg, mode_t);
		va_end(arg);
		ret = _wopen(path, flags, mode);
	} else {
		ret = _wopen(path, flags);
	}
	util_free_UTF16(path);
	/* BOM skipping should not modify errno */
	int orig_errno = errno;
	/*
	 * text files on windows can contain BOM. As we open files
	 * in binary mode we have to detect bom and skip it
	 */
	if (ret != -1) {
		char bom[3];
		if (_read(ret, bom, sizeof(bom)) != 3 ||
				memcmp(bom, UTF8_BOM, 3) != 0) {
			/* UTF-8 bom not found - reset file to the beginning */
			lseek(ret, 0, SEEK_SET);
		}
	}
	errno = orig_errno;
	return ret;
}

/*
 * os_stat -- stat abstraction layer
 */
int
os_stat(const char *pathname, os_stat_t *buf)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _wstat64(path, buf);

	util_free_UTF16(path);
	return ret;
}

/*
 * os_unlink -- unlink abstraction layer
 */
int
os_unlink(const char *pathname)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _wunlink(path);
	util_free_UTF16(path);
	return ret;
}

/*
 * os_access -- access abstraction layer
 */
int
os_access(const char *pathname, int mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _waccess(path, mode);
	util_free_UTF16(path);
	return ret;
}

/*
 * os_skipBOM -- (internal) Skip BOM in file stream
 *
 * text files on windows can contain BOM. We have to detect bom and skip it.
 */
static void
os_skipBOM(FILE *file)
{
	if (file == NULL)
		return;

	/* BOM skipping should not modify errno */
	int orig_errno = errno;
	/* UTF-8 BOM */
	uint8_t bom[3];
	size_t read_num = fread(bom, sizeof(bom[0]), sizeof(bom), file);
	if (read_num != ARRAY_SIZE(bom))
		goto out;

	if (memcmp(bom, UTF8_BOM, ARRAY_SIZE(bom)) != 0) {
		/* UTF-8 bom not found - reset file to the beginning */
		fseek(file, 0, SEEK_SET);
	}

out:
	errno = orig_errno;
}

/*
 * os_fopen -- fopen abstraction layer
 */
FILE *
os_fopen(const char *pathname, const char *mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return NULL;

	wchar_t *wmode = util_toUTF16(mode);
	if (path == NULL) {
		util_free_UTF16(path);
		return NULL;
	}

	FILE *ret = _wfopen(path, wmode);

	util_free_UTF16(path);
	util_free_UTF16(wmode);

	os_skipBOM(ret);
	return ret;
}

/*
 * os_fdopen -- fdopen abstraction layer
 */
FILE *
os_fdopen(int fd, const char *mode)
{
	FILE *ret = fdopen(fd, mode);
	os_skipBOM(ret);
	return ret;
}

/*
 * os_chmod -- chmod abstraction layer
 */
int
os_chmod(const char *pathname, mode_t mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _wchmod(path, mode);
	util_free_UTF16(path);
	return ret;
}

/*
 * os_mkstemp -- generate a unique temporary filename from template
 */
int
os_mkstemp(char *temp)
{
	unsigned rnd;
	wchar_t *utemp = util_toUTF16(temp);
	if (utemp == NULL)
		return -1;

	wchar_t *path = _wmktemp(utemp);
	if (path == NULL) {
		util_free_UTF16(utemp);
		return -1;
	}

	wchar_t npath[MAX_PATH];
	wcscpy(npath, path);

	util_free_UTF16(utemp);
	/*
	 * Use rand_s to generate more unique tmp file name than _mktemp do.
	 * In case with multiple threads and multiple files even after close()
	 * file name conflicts occurred.
	 * It resolved issue with synchronous removing
	 * multiples files by system.
	 */
	rand_s(&rnd);
	int cnt = _snwprintf(npath + wcslen(npath), MAX_PATH, L"%d", rnd);
	if (cnt < 0)
		return cnt;

	/*
	 * Use O_TEMPORARY flag to make sure the file is deleted when
	 * the last file descriptor is closed.  Also, it prevents opening
	 * this file from another process.
	 */
	return _wopen(npath, O_RDWR | O_CREAT | O_EXCL | O_TEMPORARY,
		S_IWRITE | S_IREAD);
}
