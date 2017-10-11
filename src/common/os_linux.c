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
 * os_linux.c -- Linux abstraction layer
 */

#include <fcntl.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "util.h"
#include "out.h"
#include "os.h"

/*
 * os_open -- open abstraction layer
 */
int
os_open(const char *pathname, int flags, ...)
{
	if (flags & O_CREAT) {
		va_list arg;
		va_start(arg, flags);
		mode_t mode = va_arg(arg, mode_t);
		va_end(arg);
		return open(pathname, flags, mode);
	} else {
		return open(pathname, flags);
	}
}

/*
 * os_stat -- stat abstraction layer
 */
int
os_stat(const char *pathname, os_stat_t *buf)
{
	return stat(pathname, buf);
}

/*
 * os_unlink -- unlink abstraction layer
 */
int
os_unlink(const char *pathname)
{
	return unlink(pathname);
}

/*
 * os_access -- access abstraction layer
 */
int
os_access(const char *pathname, int mode)
{
	return access(pathname, mode);
}

/*
 * os_fopen -- fopen abstraction layer
 */
FILE *
os_fopen(const char *pathname, const char *mode)
{
	return fopen(pathname, mode);
}

/*
 * os_fdopen -- fdopen abstraction layer
 */
FILE *
os_fdopen(int fd, const char *mode)
{
	return fdopen(fd, mode);
}

/*
 * os_chmod -- chmod abstraction layer
 */
int
os_chmod(const char *pathname, mode_t mode)
{
	return chmod(pathname, mode);
}

/*
 * os_mkstemp -- mkstemp abstraction layer
 */
int
os_mkstemp(char *temp)
{
	return mkstemp(temp);
}

/*
 * os_posix_fallocate -- posix_fallocate abstraction layer
 */
int
os_posix_fallocate(int fd, os_off_t offset, off_t len)
{
	return posix_fallocate(fd, offset, len);
}

/*
 * os_ftruncate -- ftruncate abstraction layer
 */
int
os_ftruncate(int fd, os_off_t length)
{
	return ftruncate(fd, length);
}

/*
 * os_flock -- flock abstraction layer
 */
int
os_flock(int fd, int operation)
{
	int opt = 0;
	if (operation & OS_LOCK_EX)
		opt |= LOCK_EX;
	if (operation & OS_LOCK_SH)
		opt |= LOCK_SH;
	if (operation & OS_LOCK_UN)
		opt |= LOCK_UN;
	if (operation & OS_LOCK_NB)
		opt |= LOCK_NB;

	return flock(fd, opt);
}

/*
 * os_writev -- writev abstraction layer
 */
ssize_t
os_writev(int fd, const struct iovec *iov, int iovcnt)
{
	return writev(fd, iov, iovcnt);
}

/*
 * os_clock_gettime -- clock_gettime abstraction layer
 */
int
os_clock_gettime(int id, struct timespec *ts)
{
	return clock_gettime(id, ts);
}

/*
 * os_rand_r -- rand_r abstraction layer
 */
int
os_rand_r(unsigned *seedp)
{
	return rand_r(seedp);
}

/*
 * os_unsetenv -- unsetenv abstraction layer
 */
int
os_unsetenv(const char *name)
{
	return unsetenv(name);
}

/*
 * os_setenv -- setenv abstraction layer
 */
int
os_setenv(const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);
}

/*
 * os_getenv -- getenv abstraction layer
 */
char *
os_getenv(const char *name)
{
	return getenv(name);
}

/*
 * os_strsignal -- strsignal abstraction layer
 */
const char *os_strsignal(int sig) {
	return strsignal(sig);
}
