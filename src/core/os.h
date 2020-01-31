/*
 * Copyright 2017-2020, Intel Corporation
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
 * os.h -- os abstraction layer
 */

#ifndef PMDK_OS_H
#define PMDK_OS_H 1

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

#include "errno_freebsd.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
#define OS_DIR_SEPARATOR '/'
#define OS_DIR_SEP_STR "/"
#else
#define OS_DIR_SEPARATOR '\\'
#define OS_DIR_SEP_STR "\\"
#endif

#ifndef _WIN32

/* madvise() */
#ifdef __FreeBSD__
#define os_madvise minherit
#define MADV_DONTFORK INHERIT_NONE
#else
#define os_madvise madvise
#endif

/* dlopen() */
#ifdef __FreeBSD__
#define RTLD_DEEPBIND 0	/* XXX */
#endif

/* major(), minor() */
#ifdef __FreeBSD__
#define os_major (unsigned)major
#define os_minor (unsigned)minor
#else
#define os_major major
#define os_minor minor
#endif

#endif /* #ifndef _WIN32 */

struct iovec;

/* os_flock */
#define OS_LOCK_SH 1
#define OS_LOCK_EX 2
#define OS_LOCK_NB 4
#define OS_LOCK_UN 8

#ifndef _WIN32
typedef struct stat os_stat_t;
#define os_fstat	fstat
#define os_lseek	lseek
#else
typedef struct _stat64 os_stat_t;
#define os_fstat	_fstat64
#define os_lseek	_lseeki64
#endif

#define os_close close
#define os_fclose fclose

#ifndef _WIN32
typedef off_t os_off_t;
#else
/* XXX: os_off_t defined in platform.h */
#endif
int os_open(const char *pathname, int flags, ...);
int os_fsync(int fd);
int os_fsync_dir(const char *dir_name);
int os_stat(const char *pathname, os_stat_t *buf);
int os_unlink(const char *pathname);
int os_access(const char *pathname, int mode);
FILE *os_fopen(const char *pathname, const char *mode);
FILE *os_fdopen(int fd, const char *mode);
int os_chmod(const char *pathname, mode_t mode);
int os_mkstemp(char *temp);
int os_posix_fallocate(int fd, os_off_t offset, os_off_t len);
int os_ftruncate(int fd, os_off_t length);
int os_flock(int fd, int operation);
ssize_t os_writev(int fd, const struct iovec *iov, int iovcnt);
int os_clock_gettime(int id, struct timespec *ts);
unsigned os_rand_r(unsigned *seedp);
int os_unsetenv(const char *name);
int os_setenv(const char *name, const char *value, int overwrite);
char *os_getenv(const char *name);
const char *os_strsignal(int sig);
int os_execv(const char *path, char *const argv[]);

/*
 * XXX: missing APis (used in ut_file.c)
 *
 * rename
 * read
 * write
 */

#ifdef __cplusplus
}
#endif

#endif /* os.h */
