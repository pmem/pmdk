// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * ut_file.c -- unit test file operations
 */

#include "unittest.h"

/*
 * ut_open -- an open that cannot return < 0
 */
int
ut_open(const char *file, int line, const char *func, const char *path,
    int flags, ...)
{
	va_list ap;
	int mode;

	va_start(ap, flags);
	mode = va_arg(ap, int);
	int retval = os_open(path, flags, mode);
	va_end(ap);

	if (retval < 0)
		ut_fatal(file, line, func, "!open: %s", path);

	return retval;
}

/*
 * ut_close -- a close that cannot return -1
 */
int
ut_close(const char *file, int line, const char *func, int fd)
{
	int retval = os_close(fd);

	if (retval != 0)
		ut_fatal(file, line, func, "!close: %d", fd);

	return retval;
}

/*
 * ut_fopen --an fopen that cannot return != 0
 */
FILE *
ut_fopen(const char *file, int line, const char *func, const char *path,
    const char *mode)
{
	FILE *retval = os_fopen(path, mode);

	if (retval == NULL)
		ut_fatal(file, line, func, "!fopen: %s", path);

	return retval;
}

/*
 * ut_fclose -- a fclose that cannot return != 0
 */
int
ut_fclose(const char *file, int line, const char *func, FILE *stream)
{
	int retval = os_fclose(stream);

	if (retval != 0)
		ut_fatal(file, line, func, "!fclose");

	return retval;
}

/*
 * ut_unlink -- an unlink that cannot return -1
 */
int
ut_unlink(const char *file, int line, const char *func, const char *path)
{
	int retval = os_unlink(path);

	if (retval != 0)
		ut_fatal(file, line, func, "!unlink: %s", path);

	return retval;
}

/*
 * ut_posix_fallocate -- a posix_fallocate that cannot return -1
 */
int
ut_posix_fallocate(const char *file, int line, const char *func, int fd,
    os_off_t offset, os_off_t len)
{
	int retval = os_posix_fallocate(fd, offset, len);

	if (retval != 0) {
		errno = retval;
		ut_fatal(file, line, func,
		    "!fallocate: fd %d offset 0x%llx len %llu",
		    fd, (unsigned long long)offset, (unsigned long long)len);
	}

	return retval;
}

/*
 * ut_write -- a write that can't return -1
 */
size_t
ut_write(const char *file, int line, const char *func, int fd,
    const void *buf, size_t count)
{
	ssize_t retval = write(fd, buf, count);
	if (retval < 0)
		ut_fatal(file, line, func, "!write: %d", fd);

	return (size_t)retval;
}

/*
 * ut_read -- a read that can't return -1
 */
size_t
ut_read(const char *file, int line, const char *func, int fd,
    void *buf, size_t count)
{
	ssize_t retval = read(fd, buf, count);
	if (retval < 0)
		ut_fatal(file, line, func, "!read: %d", fd);

	return (size_t)retval;
}

/*
 * ut_lseek -- an lseek that can't return -1
 */
os_off_t
ut_lseek(const char *file, int line, const char *func, int fd,
    os_off_t offset, int whence)
{
	os_off_t retval = os_lseek(fd, offset, whence);

	if (retval == -1)
		ut_fatal(file, line, func, "!lseek: %d", fd);

	return retval;
}

/*
 * ut_fstat -- a fstat that cannot return -1
 */
int
ut_fstat(const char *file, int line, const char *func, int fd,
    os_stat_t *st_bufp)
{
	int retval = os_fstat(fd, st_bufp);

	if (retval < 0)
		ut_fatal(file, line, func, "!fstat: %d", fd);

	return retval;
}

/*
 * ut_stat -- a stat that cannot return -1
 */
int
ut_stat(const char *file, int line, const char *func, const char *path,
    os_stat_t *st_bufp)
{
	int retval = os_stat(path, st_bufp);

	if (retval < 0)
		ut_fatal(file, line, func, "!stat: %s", path);

	return retval;
}

/*
 * ut_mmap -- a mmap call that cannot return MAP_FAILED
 */
void *
ut_mmap(const char *file, int line, const char *func, void *addr,
    size_t length, int prot, int flags, int fd, os_off_t offset)
{
	void *ret_addr = mmap(addr, length, prot, flags, fd, offset);

	if (ret_addr == MAP_FAILED) {
		const char *error = "";
		ut_fatal(file, line, func,
			"!mmap: addr=0x%llx length=0x%zx prot=%d flags=%d fd=%d offset=0x%llx %s",
			(unsigned long long)addr, length, prot,
			flags, fd, (unsigned long long)offset, error);
	}
	return ret_addr;
}

/*
 * ut_munmap -- a munmap call that cannot return -1
 */
int
ut_munmap(const char *file, int line, const char *func, void *addr,
    size_t length)
{
	int retval = munmap(addr, length);

	if (retval < 0)
		ut_fatal(file, line, func, "!munmap: addr=0x%llx length=0x%zx",
		    (unsigned long long)addr, length);

	return retval;
}

/*
 * ut_mprotect -- a mprotect call that cannot return -1
 */
int
ut_mprotect(const char *file, int line, const char *func, void *addr,
    size_t len, int prot)
{
	int retval = mprotect(addr, len, prot);

	if (retval < 0)
		ut_fatal(file, line, func,
		    "!mprotect: addr=0x%llx length=0x%zx prot=0x%x",
		    (unsigned long long)addr, len, prot);

	return retval;
}

/*
 * ut_ftruncate -- a ftruncate that cannot return -1
 */
int
ut_ftruncate(const char *file, int line, const char *func, int fd,
    os_off_t length)
{
	int retval = os_ftruncate(fd, length);

	if (retval < 0)
		ut_fatal(file, line, func, "!ftruncate: %d %llu",
				fd, (unsigned long long)length);

	return retval;
}

/*
 * file_map -- map file without using pmdk api
 */
void *
ut_file_map(const char *file, int line, const char *func, int fd, size_t size)
{
	void *addr = NULL;
	ut_mmap(file, line, func, addr, size, PROT_READ | PROT_WRITE,
		MAP_SHARED, fd, 0);
	return addr;
}
