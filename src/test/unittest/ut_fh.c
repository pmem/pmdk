/*
 * Copyright 2019-2020, Intel Corporation
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
 * ut_fh.c -- implementation of OS-independent file handle / file descriptor
 * interface
 */

/* for O_TMPFILE */
#define _GNU_SOURCE

#include <fcntl.h>

#include "ut_fh.h"
#include "unittest.h"

struct FHandle {
	int fd;
#ifdef _WIN32
	HANDLE h;
#endif
	enum file_handle_type type;
};

#ifdef _WIN32
#define HIDWORD(x) ((DWORD)((x) >> 32))
#define LODWORD(x) ((DWORD)((x) & 0xFFFFFFFF))
#endif

static void
check_invalid_flags(const char *file, int line, const char *func, int flags)
{
	if ((flags & FH_EXCL) && !(flags & FH_CREAT)) {
		ut_fatal(file, line, func,
				"FH_EXCL without FH_CREAT is meaningless");
	}

	if ((flags & FH_TRUNC) && (flags & FH_CREAT)) {
		/* because Windows doesn't support both */
		ut_fatal(file, line, func,
				"FH_TRUNC with FH_CREAT is forbidden");
	}
}

static int
ut_fh_open_fd(const char *file, int line, const char *func,
		const char *path, int flags, mode_t mode)
{
	int sflags = 0;

	check_invalid_flags(file, line, func, flags);

	if ((flags & (FH_CREAT | FH_EXCL)) == (FH_CREAT | FH_EXCL)) {
		flags &= ~(FH_CREAT | FH_EXCL);
		sflags |= O_CREAT | O_EXCL;
	} else if (flags & FH_CREAT) {
		flags &= ~FH_CREAT;
		sflags |= O_CREAT;
	/* Windows version doesn't support both O_TRUNC and O_CREAT */
	} else if (flags & FH_TRUNC) {
		flags &= ~FH_TRUNC;
		sflags |= O_TRUNC;
	}

	int acc = flags & FH_ACCMODE;

	if (acc == FH_RDONLY)
		sflags |= O_RDONLY;
	else if (acc == FH_WRONLY)
		sflags |= O_WRONLY;
	else if (acc == FH_RDWR)
		sflags |= O_RDWR;
	else
		ut_fatal(file, line, func, "unknown access mode %d", acc);

	flags &= ~FH_ACCMODE;

	if (flags & FH_DIRECTORY) {
#ifdef _WIN32
		ut_fatal(file, line, func,
				"FH_DIRECTORY is not supported on Windows using FD interface");
#else
		flags &= ~FH_DIRECTORY;
		sflags |= O_DIRECTORY;
#endif
	}

	if (flags & FH_TMPFILE) {
#ifdef O_TMPFILE
		flags &= ~FH_TMPFILE;
		sflags |= O_TMPFILE;
#else
		ut_fatal(file, line, func,
				"FH_TMPFILE is not supported on this system for file descriptors");
#endif
	}

	if (flags)
		ut_fatal(file, line, func, "unsupported flag(s) 0%o", flags);

	return ut_open(file, line, func, path, sflags, mode);
}

#ifdef _WIN32
static HANDLE
ut_fh_open_handle(const char *file, int line, const char *func,
		const char *path, int flags, mode_t mode)
{
	DWORD dwDesiredAccess;

	/* do not allow delete, read or write from another process */
	DWORD dwShareMode = 0;

	LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL;
	DWORD dwCreationDisposition;
	DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE hTemplateFile = NULL;

	/* XXX sometimes doesn't work, ERROR_ACCESS_DENIED on AppVeyor */
#if 0
	/*
	 * FILE_FLAG_DELETE_ON_CLOSE needs a real file (FH_CREAT)
	 * If it already exists refuse to use it (FH_EXCL), because this means
	 * something weird is going on (either there's another process with
	 * the same file opened or FILE_FLAG_DELETE_ON_CLOSE didn't actually
	 * delete the file on close)
	 */
	if (flags & FH_TMPFILE)
		flags |= FH_CREAT | FH_EXCL;
#else
	if (flags & FH_TMPFILE)
		ut_fatal(file, line, func,
			"FH_TMPFILE is not supported for file handles");
#endif

	check_invalid_flags(file, line, func, flags);

	/* only write permission can be taken out on Windows */
	if (!(mode & _S_IWRITE))
		dwFlagsAndAttributes |= FILE_ATTRIBUTE_READONLY;

	if ((flags & (FH_CREAT | FH_EXCL)) == (FH_CREAT | FH_EXCL)) {
		flags &= ~(FH_CREAT | FH_EXCL);
		dwCreationDisposition = CREATE_NEW;
	} else if (flags & FH_CREAT) {
		flags &= ~FH_CREAT;
		dwCreationDisposition = OPEN_ALWAYS;
	} else if (flags & FH_TRUNC) {
		flags &= ~FH_TRUNC;
		dwCreationDisposition = TRUNCATE_EXISTING;
	} else {
		dwCreationDisposition = OPEN_EXISTING;
	}

	int acc = flags & FH_ACCMODE;

	if (acc == FH_RDONLY)
		dwDesiredAccess = GENERIC_READ;
	else if (acc == FH_WRONLY)
		dwDesiredAccess = GENERIC_WRITE;
	else if (acc == FH_RDWR)
		dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
	else
		ut_fatal(file, line, func, "unknown access mode %d", acc);

	flags &= ~FH_ACCMODE;

	if (flags & FH_DIRECTORY) {
		flags &= ~FH_DIRECTORY;
		/* GJ MS */
		dwFlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
	}

	char *full_path = NULL;
	if (flags & FH_TMPFILE) {
		flags &= ~FH_TMPFILE;
		dwFlagsAndAttributes |= FILE_FLAG_DELETE_ON_CLOSE;

		/*
		 * FILE_FLAG_DELETE_ON_CLOSE needs a real file,
		 * not a directory
		 */
		full_path = MALLOC(strlen(path) + 1 +
				strlen("UT_FH_TMPFILE") + 1);
		sprintf(full_path, "%s\\UT_FH_TMPFILE", path);
		path = full_path;
	}

	if (flags)
		ut_fatal(file, line, func, "unsupported flag(s) 0%o", flags);

	wchar_t *wpath = util_toUTF16(path);
	if (wpath == NULL)
		ut_fatal(file, line, func, "conversion to utf16 failed");

	HANDLE h = CreateFileW(wpath, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile);

	util_free_UTF16(wpath);

	if (h == INVALID_HANDLE_VALUE) {
		ut_fatal(file, line, func, "opening file %s failed: %d", path,
				GetLastError());
	}

	if (full_path)
		free(full_path);

	return h;
}
#endif

struct FHandle *
ut_fh_open(const char *file, int line, const char *func,
		enum file_handle_type type, const char *path, int flags, ...)
{
	struct FHandle *f = MALLOC(sizeof(*f));

	mode_t mode = 0;

	va_list ap;

	va_start(ap, flags);
	if ((flags & FH_CREAT) || (flags & FH_TMPFILE))
		mode = va_arg(ap, mode_t);

	va_end(ap);

	f->type = type;

	if (type == FH_FD) {
		f->fd = ut_fh_open_fd(file, line, func, path, flags, mode);
	} else if (type == FH_HANDLE) {
#ifdef _WIN32
		f->h = ut_fh_open_handle(file, line, func, path, flags,
				mode);
#else
		ut_fatal(file, line, func,
				"FH_HANDLE not supported on !Windows");
#endif
	} else {
		ut_fatal(file, line, func, "unknown type value %d", type);
	}

	return f;
}

void
ut_fh_truncate(const char *file, int line, const char *func,
		struct FHandle *f, os_off_t length)
{
	if (f->type == FH_FD) {
		ut_ftruncate(file, line, func, f->fd, length);
	} else if (f->type == FH_HANDLE) {
#ifdef _WIN32
		LONG low = LODWORD(length);
		LONG high = HIDWORD(length);
		if (SetFilePointer(f->h, low, &high, FILE_BEGIN) ==
				INVALID_SET_FILE_POINTER &&
				GetLastError() != ERROR_SUCCESS) {
			ut_fatal(file, line, func, "SetFilePointer failed: %d",
					GetLastError());
		}

		if (SetEndOfFile(f->h) == 0) {
			ut_fatal(file, line, func, "SetEndOfFile failed: %d",
					GetLastError());
		}
#else
		ut_fatal(file, line, func,
				"FH_HANDLE not supported on !Windows");
#endif
	} else {
		ut_fatal(file, line, func, "unknown type value %d", f->type);
	}
}

void
ut_fh_close(const char *file, int line, const char *func, struct FHandle *f)
{
	if (f->type == FH_FD) {
		CLOSE(f->fd);
	} else if (f->type == FH_HANDLE) {
#ifdef _WIN32
		CloseHandle(f->h);
#else
		ut_fatal(file, line, func,
				"FH_HANDLE not supported on !Windows");
#endif
	} else {
		ut_fatal(file, line, func, "unknown type value %d", f->type);
	}

	memset(f, 0, sizeof(*f));
	FREE(f);
}

int
ut_fh_get_fd(const char *file, int line, const char *func, struct FHandle *f)
{
	if (f->type == FH_FD)
		return f->fd;
	ut_fatal(file, line, func,
		"requested file descriptor on FHandle that doesn't contain it");
}

#ifdef _WIN32
HANDLE
ut_fh_get_handle(const char *file, int line, const char *func,
		struct FHandle *f)
{
	if (f->type == FH_HANDLE)
		return f->h;
	ut_fatal(file, line, func,
		"requested file handle on FHandle that doesn't contain it");
}
#endif

enum file_handle_type
ut_fh_get_handle_type(struct FHandle *fh)
{
	return fh->type;
}
