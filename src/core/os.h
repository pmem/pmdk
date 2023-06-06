/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2023, Intel Corporation */

/*
 * os.h -- os abstraction layer
 */

#ifndef PMDK_OS_H
#define PMDK_OS_H 1

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS_DIR_SEPARATOR '/'
#define OS_DIR_SEP_STR "/"

/* madvise() */
#define os_madvise madvise

/* major(), minor() */
#define os_major major
#define os_minor minor

struct iovec;

/* os_flock */
#define OS_LOCK_SH 1
#define OS_LOCK_EX 2
#define OS_LOCK_NB 4
#define OS_LOCK_UN 8

typedef struct stat os_stat_t;
#define os_fstat	fstat
#define os_lseek	lseek

#define os_close close
#define os_fclose fclose

typedef off_t os_off_t;
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
