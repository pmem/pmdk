// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

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
	enum file_handle_type type;
};

static void
check_invalid_flags(const char *file, int line, const char *func, int flags)
{
	if ((flags & FH_EXCL) && !(flags & FH_CREAT)) {
		ut_fatal(file, line, func,
				"FH_EXCL without FH_CREAT is meaningless");
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
	}

	int acc = flags & FH_ACCMODE;

	/* Linux version does not have FH_EXEC equivalent */
	if ((acc & FH_WRITE) && (acc & FH_READ))
		sflags |= O_RDWR;
	else if (acc & FH_WRITE)
		sflags |= O_WRONLY;
	else if (acc & FH_READ)
		sflags |= O_RDONLY;
	else
		ut_fatal(file, line, func, "unknown access mode %d", acc);

	flags &= ~FH_ACCMODE;

	if (flags & FH_DIRECTORY) {
		flags &= ~FH_DIRECTORY;
		sflags |= O_DIRECTORY;
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
	} else {
		ut_fatal(file, line, func, "unknown type value %d", f->type);
	}
}

void
ut_fh_close(const char *file, int line, const char *func, struct FHandle *f)
{
	if (f->type == FH_FD) {
		CLOSE(f->fd);
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

enum file_handle_type
ut_fh_get_handle_type(struct FHandle *fh)
{
	return fh->type;
}
