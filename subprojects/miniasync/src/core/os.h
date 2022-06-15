/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2022, Intel Corporation */

/*
 * os.h -- os abstraction layer
 */

#ifndef MINIASYNC_OS_H
#define MINIASYNC_OS_H 1

#include <sys/stat.h>
#include <stdio.h>

#include "errno_freebsd.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "windows/include/unistd.h"
#include "windows/include/platform.h"
#else
#include <unistd.h>
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

#define os_close close
#define os_fclose fclose

#ifndef _WIN32
typedef off_t os_off_t;
#else
/* XXX: os_off_t defined in platform.h */
#endif
int os_open(const char *pathname, int flags, ...);
int os_unlink(const char *pathname);
FILE *os_fopen(const char *pathname, const char *mode);
int os_mkstemp(char *temp);
int os_ftruncate(int fd, os_off_t length);
int os_flock(int fd, int operation);
ssize_t os_writev(int fd, const struct iovec *iov, int iovcnt);
int os_clock_gettime(int id, struct timespec *ts);
unsigned os_rand_r(unsigned *seedp);
int os_unsetenv(const char *name);
int os_setenv(const char *name, const char *value, int overwrite);
char *os_getenv(const char *name);
const char *os_strsignal(int sig);

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
