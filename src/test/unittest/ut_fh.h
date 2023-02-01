/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2019-2023, Intel Corporation */

/*
 * ut_fh.h -- OS-independent file handle / file descriptor interface
 */

#ifndef UT_FH_H
#define UT_FH_H

#include "os.h"

struct FHandle;

/* XXX  https://github.com/pmem/pmdk/issues/5711 */
enum file_handle_type { FH_FD };

#define FH_ACCMODE	(7)
#define FH_READ		(1 << 0)
#define FH_WRITE	(1 << 1)
#define FH_RDWR		(FH_READ | FH_WRITE)
#define FH_EXEC		(1 << 2)

#define FH_CREAT	(1 << 3)
#define FH_EXCL		(1 << 4)
#define FH_TRUNC	(1 << 5)

/* needs directory */
#define FH_TMPFILE	(1 << 6)

#define FH_DIRECTORY	(1 << 7)

#define UT_FH_OPEN(type, path, flags, ...)				\
	ut_fh_open(__FILE__, __LINE__, __func__, type, path,		\
			flags, ##__VA_ARGS__)

#define UT_FH_TRUNCATE(fhandle, size)					\
	ut_fh_truncate(__FILE__, __LINE__, __func__, fhandle, size)

#define UT_FH_GET_FD(fhandle)						\
	ut_fh_get_fd(__FILE__, __LINE__, __func__, fhandle)

#define UT_FH_CLOSE(fhandle)						\
	ut_fh_close(__FILE__, __LINE__, __func__, fhandle)

struct FHandle *ut_fh_open(const char *file, int line, const char *func,
		enum file_handle_type type, const char *path, int flags, ...);

void ut_fh_truncate(const char *file, int line, const char *func,
		struct FHandle *f, os_off_t length);

void ut_fh_close(const char *file, int line, const char *func,
		struct FHandle *f);

enum file_handle_type ut_fh_get_handle_type(struct FHandle *fh);

int ut_fh_get_fd(const char *file, int line, const char *func,
		struct FHandle *f);

#endif
