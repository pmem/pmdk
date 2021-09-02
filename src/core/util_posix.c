// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * util_posix.c -- Abstraction layer for misc utilities (Posix implementation)
 */

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "os.h"
#include "out.h"
#include "util.h"

/* pass through for Posix */
void
util_strerror(int errnum, char *buff, size_t bufflen)
{
	strerror_r(errnum, buff, bufflen);
}

/*
 * util_strwinerror -- should never be called on posix OS - abort()
 */
void
util_strwinerror(unsigned long err, char *buff, size_t bufflen)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(err, buff, bufflen);
	abort();
}

/*
 * util_part_realpath -- get canonicalized absolute pathname
 *
 * As paths used in a poolset file have to be absolute (checked when parsing
 * a poolset file), here we only have to resolve symlinks.
 */
char *
util_part_realpath(const char *path)
{
	return realpath(path, NULL);
}

/*
 * util_compare_file_inodes -- compare device and inodes of two files;
 *                             this resolves hard links
 */
int
util_compare_file_inodes(const char *path1, const char *path2)
{
	struct stat sb1, sb2;
	if (os_stat(path1, &sb1)) {
		if (errno != ENOENT) {
			ERR("!stat failed for %s", path1);
			return -1;
		}
		LOG(1, "stat failed for %s", path1);
		errno = 0;
		return strcmp(path1, path2) != 0;
	}

	if (os_stat(path2, &sb2)) {
		if (errno != ENOENT) {
			ERR("!stat failed for %s", path2);
			return -1;
		}
		LOG(1, "stat failed for %s", path2);
		errno = 0;
		return strcmp(path1, path2) != 0;
	}

	return sb1.st_dev != sb2.st_dev || sb1.st_ino != sb2.st_ino;
}

/*
 * util_tmpfile_mkstemp --  (internal) create temporary file
 *                          if O_TMPFILE not supported
 */
static int
util_tmpfile_mkstemp(const char *dir, const char *templ)
{
	/* the templ must start with a path separator */
	ASSERTeq(templ[0], '/');

	int oerrno;
	int fd = -1;

	char *fullname = alloca(strlen(dir) + strlen(templ) + 1);

	(void) strcpy(fullname, dir);
	(void) strcat(fullname, templ);

	sigset_t set, oldset;
	sigfillset(&set);
	(void) sigprocmask(SIG_BLOCK, &set, &oldset);

	mode_t prev_umask = umask(S_IRWXG | S_IRWXO);

	fd = os_mkstemp(fullname);

	umask(prev_umask);

	if (fd < 0) {
		ERR("!mkstemp");
		goto err;
	}

	(void) os_unlink(fullname);
	(void) sigprocmask(SIG_SETMASK, &oldset, NULL);
	LOG(3, "unlinked file is \"%s\"", fullname);

	return fd;

err:
	oerrno = errno;
	(void) sigprocmask(SIG_SETMASK, &oldset, NULL);
	if (fd != -1)
		(void) os_close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_tmpfile -- create temporary file
 */
int
util_tmpfile(const char *dir, const char *templ, int flags)
{
	LOG(3, "dir \"%s\" template \"%s\" flags %x", dir, templ, flags);

	/* only O_EXCL is allowed here */
	ASSERT(flags == 0 || flags == O_EXCL);

#ifdef O_TMPFILE
	int fd = os_open(dir, O_TMPFILE | O_RDWR | flags, S_IRUSR | S_IWUSR);
	/*
	 * Open can fail if underlying file system does not support O_TMPFILE
	 * flag.
	 */
	if (fd >= 0)
		return fd;
	if (errno != EOPNOTSUPP) {
		ERR("!open");
		return -1;
	}
#endif

	return util_tmpfile_mkstemp(dir, templ);
}

/*
 * util_aligned_malloc -- allocate aligned memory
 */
void *
util_aligned_malloc(size_t alignment, size_t size)
{
	void *retval = NULL;

	errno = posix_memalign(&retval, alignment, size);

	return retval;
}

/*
 * util_aligned_free -- free allocated memory in util_aligned_malloc
 */
void
util_aligned_free(void *ptr)
{
	free(ptr);
}

/*
 * util_getexecname -- return name of current executable
 */
char *
util_getexecname(char *path, size_t pathlen)
{
	ASSERT(pathlen != 0);
	ssize_t cc;

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>

	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};

	cc = (sysctl(mib, 4, path, &pathlen, NULL, 0) == -1) ?
		-1 : (ssize_t)pathlen;
#else
	cc = readlink("/proc/self/exe", path, pathlen);
#endif
	if (cc == -1) {
		strncpy(path, "unknown", pathlen);
		path[pathlen - 1] = '\0';
	} else {
		path[cc] = '\0';
	}

	return path;
}
