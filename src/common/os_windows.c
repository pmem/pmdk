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

/* UTF-8 bom (EF BB BF) */
#define UTF8_BOM "\xEF\xBB\xBF"

/*
 * os_open -- open abstraction layer
 */
int
os_open(const char *pathname, int flags, ...)
{
	wchar_t *path = util_toUTF16(pathname);
	int ret = 0;
	if (path == NULL) {
		return -1;
	}
	/* there is no O_TMPFILE on windows */
	if (flags & O_CREAT) {
		va_list arg;
		va_start(arg, flags);
		mode_t mode = va_arg(arg, mode_t);
		va_end(arg);
		ret = _wopen(path, flags, mode);
	} else {
		ret = _wopen(path, flags);
	}
	Free(path);
	/* BOM skipping should not modify errno */
	int orig_errno = errno;
	if (ret != -1) {
		char bom[3];
		if (_read(ret, bom, 3) != 3 || memcmp(bom, UTF8_BOM, 3) != 0) {
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
	if (path == NULL) {
		return -1;
	}

	int ret = _wstat64(path, buf);

	Free(path);
	return ret;
}

/*
 * os_unlink -- unlink abstraction layer
 */
int
os_unlink(const char *pathname)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL) {
		return -1;
	}

	int ret = _wunlink(path);
	Free(path);
	return ret;
}

/*
 * os_access -- access abstraction layer
 */
int
os_access(const char *pathname, int mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL) {
		return -1;
	}

	int ret = _waccess(path, mode);
	Free(path);
	return ret;
}

/*
 * os_skipBOM -- (internal) Skip BOM in file stream
 */
void
os_skipBOM(FILE *file)
{
	if (file == NULL)
		return;
	/* BOM skipping should not modify errno */
	int orig_errno = errno;
	/* UTF-8 BOM + \0 */
	char bom[4];
	fgets(bom, 4, file);

	if (strcmp(bom, UTF8_BOM) != 0) {
		/* UTF-8 bom not found - reset file to the beginning */
		fseek(file, 0, SEEK_SET);
	}
	errno = orig_errno;
}

/*
 * os_fdopen -- fdopen abstraction layer
 */
FILE *
os_fopen(const char *pathname, const char *mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL) {
		return NULL;
	}

	wchar_t *wmode = util_toUTF16(mode);
	if (path == NULL) {
		Free(path);
		return NULL;
	}

	FILE *ret = _wfopen(path, wmode);

	Free(path);
	Free(wmode);

	os_skipBOM(ret);
	return ret;
}

/*
 * os_chmod -- chmod abstraction layer
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
	if (path == NULL) {
		return -1;
	}

	int ret = _wchmod(path, mode);
	Free(path);
	return ret;
}

/*
 * os_mkstemp -- generate a unique temporary filename from template
 */
int
os_mkstemp(char *temp)
{
	unsigned rnd;
	wchar_t *_temp = util_toUTF16(temp);
	if (_temp == NULL)
		return -1;
	wchar_t *path = _wmktemp(_temp);

	if (path == NULL) {
		Free(_temp);
		return -1;
	}

	wchar_t npath[MAX_PATH];
	wcscpy(npath, path);

	Free(_temp);
	/*
	 * Use rand_s to generate more unique tmp file name than _mktemp do.
	 * In case with multiple threads and multiple files even after close()
	 * file name conflicts occurred.
	 * It resolved issue with synchronous removing
	 * multiples files by system.
	 */
	rand_s(&rnd);
	_snwprintf(npath + wcslen(npath), MAX_PATH, L"%d", rnd);

	/*
	 * Use O_TEMPORARY flag to make sure the file is deleted when
	 * the last file descriptor is closed.  Also, it prevents opening
	 * this file from another process.
	 */
	return _wopen(npath, O_RDWR | O_CREAT | O_EXCL | O_TEMPORARY,
		S_IWRITE | S_IREAD);
}
