// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2022, Intel Corporation */

/*
 * util_windows.c -- misc utilities with OS-specific implementation
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <tchar.h>

#include "util.h"
#include "out.h"
#include "os.h"

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
 * util_strwinerror -- return string describing windows error codes
 */
void
util_strwinerror(unsigned long err, char *buff, size_t bufflen)
{
	wchar_t *error_str;

	if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		0,
		(LPWSTR)&error_str,
		0, NULL) == 0) {
		sprintf_s(buff, bufflen, "GetLastError() ==  %lu", err);
		return;
	}

	if (util_toUTF8_buff(error_str, buff, bufflen)) {
		LocalFree(error_str);
		sprintf_s(buff, bufflen, "GetLastError() ==  %lu", err);
		return;
	}

	/* let's do the error message without '\r' and '\n' at the end */
	size_t len = strlen(buff);
	for (size_t i = len - 1; i > 0; i--) {
		if (buff[i] != '\r' && buff[i] != '\n') {
			buff[i + 1] = '\0';
			break;
		}
	}

	LocalFree(error_str);
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
 * util_tmpfile -- create a temporary file
 */
int
util_tmpfile(const char *dir, const char *templ, int flags)
{
	LOG(3, "dir \"%s\" template \"%s\" flags %x", dir, templ, flags);

	/* only O_EXCL is allowed here */
	ASSERT(flags == 0 || flags == O_EXCL);

	int oerrno;
	int fd = -1;

	size_t len = strlen(dir) + strlen(templ) + 1;
	char *fullname = malloc(sizeof(*fullname) * len);
	if (fullname == NULL) {
		ERR("!malloc");
		return -1;
	}

	int ret = _snprintf(fullname, len, "%s%s", dir, templ);
	if (ret < 0 || (size_t)ret >= len) {
		ERR("snprintf: %d", ret);
		goto err;
	}

	LOG(4, "fullname \"%s\"", fullname);

	/*
	 * XXX - block signals and modify file creation mask for the time
	 * of mkstmep() execution.  Restore previous settings once the file
	 * is created.
	 */

	fd = os_mkstemp(fullname);
	if (fd < 0) {
		ERR("!os_mkstemp");
		goto err;
	}

	/*
	 * There is no point to use unlink() here.  First, because it does not
	 * work on open files.  Second, because the file is created with
	 * O_TEMPORARY flag, and it looks like such temp files cannot be open
	 * from another process, even though they are visible on
	 * the filesystem.
	 */

	free(fullname);
	return fd;

err:
	free(fullname);
	oerrno = errno;
	if (fd != -1)
		(void) os_close(fd);
	errno = oerrno;
	return -1;
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

	char *str = malloc(size * sizeof(char));
	if (str == NULL)
		goto out;

	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1, str,
		size, NULL, NULL) == 0) {
		free(str);
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
	free(str);
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

	wchar_t *wstr = malloc(size * sizeof(wchar_t));
	if (wstr == NULL)
		goto out;

	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, wstr,
		size) == 0) {
		free(wstr);
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
	free(wstr);
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
	if (size == 0 || out_size < (size_t)size)
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
	if (size == 0 || out_size < (size_t)size)
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

/*
 * util_suppress_errmsg -- suppresses "abort" window on Windows if env variable
 * is set, useful for automatic tests
 */
void
util_suppress_errmsg(void)
{
	if (os_getenv("PMDK_NO_ABORT_MSG") != NULL) {
		DWORD err = GetErrorMode();
		SetErrorMode(err | SEM_NOGPFAULTERRORBOX |
			SEM_FAILCRITICALERRORS);
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	}
}

static int Lasterror_to_errno[16000] = {
	[ERROR_ACCESS_DENIED] = EACCES,
	[ERROR_FILE_NOT_FOUND] = ENOENT,
	[ERROR_INVALID_ACCESS] = EACCES,
	[ERROR_INVALID_ADDRESS] = EINVAL,
	[ERROR_INVALID_FUNCTION] = EINVAL,
	[ERROR_INVALID_HANDLE] = EINVAL,
	[ERROR_INVALID_PARAMETER] = EINVAL,
	[ERROR_LOCK_FAILED] = EACCES,
	[ERROR_MAPPED_ALIGNMENT] = EINVAL,
	[ERROR_NOT_ENOUGH_MEMORY] = ENOMEM,
	[ERROR_NOT_SUPPORTED] = ENOTSUP,
	[ERROR_OUTOFMEMORY] = ENOMEM,
	[ERROR_PATH_NOT_FOUND] = ENOENT,
	[ERROR_TOO_MANY_OPEN_FILES] = EMFILE,
};

/*
 * util_lasterror_to_errno - converts windows error codes to errno
 */
int
util_lasterror_to_errno(unsigned long err)
{
	if (err >= ARRAY_SIZE(Lasterror_to_errno))
		return -1;

	/* no error */
	if (err == 0)
		return 0;

	int ret = Lasterror_to_errno[err];

	/* 0 is used to signalize missing entry in Lasterror_to_errno array */
	if (ret == 0)
		return -1;

	return ret;
}
