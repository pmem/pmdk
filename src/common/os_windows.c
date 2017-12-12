/*
 * Copyright 2017, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * os_windows.c -- windows abstraction layer
 */
#include <io.h>
#include <sys/locking.h>
#include <errno.h>
#include <pmemcompat.h>
#include "util.h"
#include "os.h"
#include "out.h"

#define UTF8_BOM "\xEF\xBB\xBF"

/*
 * os_open -- open abstraction layer
 */
int
os_open(const char *pathname, int flags, ...)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret;

	if (flags & O_CREAT) {
		va_list arg;
		va_start(arg, flags);
		mode_t mode = va_arg(arg, mode_t);
		va_end(arg);
		ret = _wopen(path, flags, mode);
	} else {
		ret = _wopen(path, flags);
	}
	util_free_UTF16(path);
	/* BOM skipping should not modify errno */
	int orig_errno = errno;
	/*
	 * text files on windows can contain BOM. As we open files
	 * in binary mode we have to detect bom and skip it
	 */
	if (ret != -1) {
		char bom[3];
		if (_read(ret, bom, sizeof(bom)) != 3 ||
				memcmp(bom, UTF8_BOM, 3) != 0) {
			/* UTF-8 bom not found - reset file to the beginning */
			lseek(ret, 0, SEEK_SET);
		}
	}
	errno = orig_errno;
	return ret;
}

/*
 * os_stat -- stat abstraction layer
 */
int
os_stat(const char *pathname, os_stat_t *buf)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _wstat64(path, buf);

	util_free_UTF16(path);
	return ret;
}

/*
 * os_unlink -- unlink abstraction layer
 */
int
os_unlink(const char *pathname)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _wunlink(path);
	util_free_UTF16(path);
	return ret;
}

/*
 * os_access -- access abstraction layer
 */
int
os_access(const char *pathname, int mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _waccess(path, mode);
	util_free_UTF16(path);
	return ret;
}

/*
 * os_skipBOM -- (internal) Skip BOM in file stream
 *
 * text files on windows can contain BOM. We have to detect bom and skip it.
 */
static void
os_skipBOM(FILE *file)
{
	if (file == NULL)
		return;

	/* BOM skipping should not modify errno */
	int orig_errno = errno;
	/* UTF-8 BOM */
	uint8_t bom[3];
	size_t read_num = fread(bom, sizeof(bom[0]), sizeof(bom), file);
	if (read_num != ARRAY_SIZE(bom))
		goto out;

	if (memcmp(bom, UTF8_BOM, ARRAY_SIZE(bom)) != 0) {
		/* UTF-8 bom not found - reset file to the beginning */
		fseek(file, 0, SEEK_SET);
	}

out:
	errno = orig_errno;
}

/*
 * os_fopen -- fopen abstraction layer
 */
FILE *
os_fopen(const char *pathname, const char *mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return NULL;

	wchar_t *wmode = util_toUTF16(mode);
	if (path == NULL) {
		util_free_UTF16(path);
		return NULL;
	}

	FILE *ret = _wfopen(path, wmode);

	util_free_UTF16(path);
	util_free_UTF16(wmode);

	os_skipBOM(ret);
	return ret;
}

/*
 * os_fdopen -- fdopen abstraction layer
 */
FILE *
os_fdopen(int fd, const char *mode)
{
	FILE *ret = fdopen(fd, mode);
	os_skipBOM(ret);
	return ret;
}

/*
 * os_chmod -- chmod abstraction layer
 */
int
os_chmod(const char *pathname, mode_t mode)
{
	wchar_t *path = util_toUTF16(pathname);
	if (path == NULL)
		return -1;

	int ret = _wchmod(path, mode);
	util_free_UTF16(path);
	return ret;
}

/*
 * os_mkstemp -- generate a unique temporary filename from template
 */
int
os_mkstemp(char *temp)
{
	unsigned rnd;
	wchar_t *utemp = util_toUTF16(temp);
	if (utemp == NULL)
		return -1;

	wchar_t *path = _wmktemp(utemp);
	if (path == NULL) {
		util_free_UTF16(utemp);
		return -1;
	}

	wchar_t *npath = Malloc(sizeof(*npath) * wcslen(path) + _MAX_FNAME);
	if (npath == NULL) {
		util_free_UTF16(utemp);
		return -1;
	}

	wcscpy(npath, path);

	util_free_UTF16(utemp);
	/*
	 * Use rand_s to generate more unique tmp file name than _mktemp do.
	 * In case with multiple threads and multiple files even after close()
	 * file name conflicts occurred.
	 * It resolved issue with synchronous removing
	 * multiples files by system.
	 */
	rand_s(&rnd);

	int ret = _snwprintf(npath + wcslen(npath), _MAX_FNAME, L"%u", rnd);
	if (ret < 0)
		goto out;

	/*
	 * Use O_TEMPORARY flag to make sure the file is deleted when
	 * the last file descriptor is closed.  Also, it prevents opening
	 * this file from another process.
	 */
	ret = _wopen(npath, O_RDWR | O_CREAT | O_EXCL | O_TEMPORARY,
		S_IWRITE | S_IREAD);

out:
	Free(npath);
	return ret;
}


/*
 * os_posix_fallocate -- allocate file space
 */
int
os_posix_fallocate(int fd, os_off_t offset, os_off_t len)
{
	/*
	 * From POSIX:
	 * "EINVAL -- The len argument was zero or the offset argument was
	 * less than zero."
	 *
	 * From Linux man-page:
	 * "EINVAL -- offset was less than 0, or len was less than or
	 * equal to 0"
	 */
	if (offset < 0 || len <= 0)
		return EINVAL;

	/*
	 * From POSIX:
	 * "EFBIG -- The value of offset+len is greater than the maximum
	 * file size."
	 *
	 * Overflow can't be checked for by _chsize_s, since it only gets
	 * the sum.
	 */
	if (offset + len < offset)
		return EFBIG;

	/*
	 * posix_fallocate should not clobber errno, but
	 * _filelengthi64 might set errno.
	 */
	int orig_errno = errno;

	__int64 current_size = _filelengthi64(fd);

	int file_length_errno = errno;
	errno = orig_errno;

	if (current_size < 0)
		return file_length_errno;

	__int64 requested_size = offset + len;

	if (requested_size <= current_size)
		return 0;

	return _chsize_s(fd, requested_size);
}

/*
 * os_ftruncate -- truncate a file to a specified length
 */
int
os_ftruncate(int fd, os_off_t length)
{
	return _chsize_s(fd, length);
}

/*
 * os_flock -- apply or remove an advisory lock on an open file
 */
int
os_flock(int fd, int operation)
{
	int flags = 0;
	SYSTEM_INFO  systemInfo;

	GetSystemInfo(&systemInfo);

	switch (operation & (OS_LOCK_EX | OS_LOCK_SH | OS_LOCK_UN)) {
		case OS_LOCK_EX:
		case OS_LOCK_SH:
			if (operation & OS_LOCK_NB)
				flags = _LK_NBLCK;
			else
				flags = _LK_LOCK;
			break;

		case OS_LOCK_UN:
			flags = _LK_UNLCK;
			break;

		default:
			errno = EINVAL;
			return -1;
	}

	os_off_t filelen = _filelengthi64(fd);
	if (filelen < 0)
		return -1;

	/* for our purpose it's enough to lock the first page of the file */
	long len = (filelen > systemInfo.dwPageSize) ?
				systemInfo.dwPageSize : (long)filelen;

	int res = _locking(fd, flags, len);
	if (res != 0 && errno == EACCES)
		errno = EWOULDBLOCK; /* for consistency with flock() */

	return res;
}

/*
 * os_writev -- windows version of writev function
 *
 * XXX: _write and other similar functions are 32 bit on windows
 *	if size of data is bigger then 2^32, this function
 *	will be not atomic.
 */
ssize_t
os_writev(int fd, const struct iovec *iov, int iovcnt)
{
	size_t size = 0;

	/* XXX: _write is 32 bit on windows */
	for (int i = 0; i < iovcnt; i++)
		size += iov[i].iov_len;

	void *buf = malloc(size);
	if (buf == NULL)
		return ENOMEM;

	char *it_buf = buf;
	for (int i = 0; i < iovcnt; i++) {
		memcpy(it_buf, iov[i].iov_base, iov[i].iov_len);
		it_buf += iov[i].iov_len;
	}

	ssize_t written = 0;
	while (size > 0) {
		int ret = _write(fd, buf, size >= MAXUINT ?
				MAXUINT : (unsigned)size);
		if (ret == -1) {
			written = -1;
			break;
		}
		written += ret;
		size -= ret;
	}

	free(buf);
	return written;
}

#define NSEC_IN_SEC 1000000000ull
/* number of useconds between 1970-01-01T00:00:00Z and 1601-01-01T00:00:00Z */
#define DELTA_WIN2UNIX (11644473600000000ull)

/*
 * clock_gettime -- returns elapsed time since the system was restarted
 * or since Epoch, depending on the mode id
 */
int
os_clock_gettime(int id, struct timespec *ts)
{
	switch (id) {
	case CLOCK_MONOTONIC:
		{
			LARGE_INTEGER time;
			LARGE_INTEGER frequency;

			QueryPerformanceFrequency(&frequency);
			QueryPerformanceCounter(&time);

			ts->tv_sec = time.QuadPart / frequency.QuadPart;
			ts->tv_nsec = (long)(
				(time.QuadPart % frequency.QuadPart) *
				NSEC_IN_SEC / frequency.QuadPart);
		}
		break;

	case CLOCK_REALTIME:
		{
			FILETIME ctime_ft;
			GetSystemTimeAsFileTime(&ctime_ft);
			ULARGE_INTEGER ctime = {
				.HighPart = ctime_ft.dwHighDateTime,
				.LowPart = ctime_ft.dwLowDateTime,
			};
			ts->tv_sec = (ctime.QuadPart - DELTA_WIN2UNIX * 10)
				/ 10000000;
			ts->tv_nsec = ((ctime.QuadPart - DELTA_WIN2UNIX * 10)
				% 10000000) * 100;
		}
		break;

	default:
		SetLastError(EINVAL);
		return -1;
	}

	return 0;
}


/*
 * os_setenv -- change or add an environment variable
 */
int
os_setenv(const char *name, const char *value, int overwrite)
{
	errno_t err;

	/*
	 * If caller doesn't want to overwrite make sure that a environment
	 * variable with the same name doesn't exist.
	 */
	if (!overwrite && getenv(name))
		return 0;

	/*
	 * _putenv_s returns a non-zero error code on failure but setenv
	 * needs to return -1 on failure, let's translate the error code.
	 */
	if ((err = _putenv_s(name, value)) != 0) {
		errno = err;
		return -1;
	}

	return 0;
}

/*
 * os_unsetenv -- remove an environment variable
 */
int
os_unsetenv(const char *name)
{
	errno_t err;
	if ((err = _putenv_s(name, "")) != 0) {
		errno = err;
		return -1;
	}

	return 0;
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
 * rand_r -- rand_r for windows
 *
 * XXX: RAND_MAX is equal 0x7fff on Windows, so to get 32 bit random number
 *	we need to merge two numbers returned by rand_s().
 *	It is not to the best solution as subsequences returned by rand_s are
 *	not guaranteed to be independent.
 *
 * XXX: Windows doesn't implement deterministic thread-safe pseudorandom
 *	generator (generator which can be initialized by seed ).
 *	We have to chose between a deterministic nonthread-safe generator
 *	(rand(), srand()) or a non-deterministic thread-safe generator(rand_s())
 *	as thread-safety is more important, a seed parameter is ignored in this
 *	implementation.
 */
int
os_rand_r(unsigned *seedp)
{
	UNREFERENCED_PARAMETER(seedp);
	unsigned part1, part2;
	rand_s(&part1);
	rand_s(&part2);
	return part1 << 16 | part2;
}

/*
 * sys_siglist -- map of signal to human readable messages like sys_siglist
 */
const char * const sys_siglist[] = {
	"Unknown signal 0",		/*  0 */
	"Hangup",			/*  1 */
	"Interrupt",			/*  2 */
	"Quit",				/*  3 */
	"Illegal instruction",		/*  4 */
	"Trace/breakpoint trap",	/*  5 */
	"Aborted",			/*  6 */
	"Bus error",			/*  7 */
	"Floating point exception",	/*  8 */
	"Killed",			/*  9 */
	"User defined signal 1",	/* 10 */
	"Segmentation fault",		/* 11 */
	"User defined signal 2",	/* 12 */
	"Broken pipe",			/* 13 */
	"Alarm clock",			/* 14 */
	"Terminated",			/* 15 */
	"Stack fault",			/* 16 */
	"Child exited",			/* 17 */
	"Continued",			/* 18 */
	"Stopped (signal)",		/* 19 */
	"Stopped",			/* 20 */
	"Stopped (tty input)",		/* 21 */
	"Stopped (tty output)",		/* 22 */
	"Urgent I/O condition",		/* 23 */
	"CPU time limit exceeded",	/* 24 */
	"File size limit exceeded",	/* 25 */
	"Virtual timer expired",	/* 26 */
	"Profiling timer expired",	/* 27 */
	"Window changed",		/* 28 */
	"I/O possible",			/* 29 */
	"Power failure",		/* 30 */
	"Bad system call",		/* 31 */
	"Unknown signal 32"		/* 32 */
};
int sys_siglist_size = ARRAYSIZE(sys_siglist);

/*
 * string constants for strsignal
 * XXX: ideally this should have the signal number as the suffix but then we
 * should use a buffer from thread local storage, so deferring the same till
 * we need it
 * NOTE: In Linux strsignal uses TLS for the same reason but if it fails to get
 * a thread local buffer it falls back to using a static buffer trading the
 * thread safety.
 */
#define STR_REALTIME_SIGNAL	"Real-time signal"
#define STR_UNKNOWN_SIGNAL	"Unknown signal"

/*
 * strsignal -- returns a string describing the signal number 'sig'
 *
 * XXX: According to POSIX, this one is of type 'char *', but in our
 * implementation it returns 'const char *'.
 */
const char *
os_strsignal(int sig)
{
	if (sig >= 0 && sig < ARRAYSIZE(sys_siglist))
		return sys_siglist[sig];
	else if (sig >= 34 && sig <= 64)
		return STR_REALTIME_SIGNAL;
	else
		return STR_UNKNOWN_SIGNAL;
}
