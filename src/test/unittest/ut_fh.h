/*
 * Copyright 2019, Intel Corporation
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
 * ut_fh.h -- OS-independent file handle / file descriptor interface
 */

#ifndef UT_FH_H
#define UT_FH_H

#include "os.h"

struct FHandle;

enum file_handle_type { FH_FD, FH_HANDLE };

#define FH_ACCMODE	(3)
#define FH_RDONLY	(0)
#define FH_WRONLY	(1)
#define FH_RDWR		(2)

#define FH_CREAT	(1 << 2)
#define FH_EXCL		(1 << 3)
#define FH_TRUNC	(1 << 4)

/* needs directory, on Windows it creates publicly visible file */
#define FH_TMPFILE	(1 << 5)

#define FH_DIRECTORY	(1 << 6)

#define UT_FH_OPEN(type, path, flags, ...)				\
	ut_fh_open(__FILE__, __LINE__, __func__, type, path,		\
			flags, ##__VA_ARGS__)

#define UT_FH_TRUNCATE(fhandle, size)					\
	ut_fh_truncate(__FILE__, __LINE__, __func__, fhandle, size)

#define UT_FH_GET_FD(fhandle)						\
	ut_fh_get_fd(__FILE__, __LINE__, __func__, fhandle)

#ifdef _WIN32
#define UT_FH_GET_HANDLE(fhandle)					\
	ut_fh_get_handle(__FILE__, __LINE__, __func__, fhandle)
#endif

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
#ifdef _WIN32
HANDLE ut_fh_get_handle(const char *file, int line, const char *func,
		struct FHandle *f);
#endif

#endif
