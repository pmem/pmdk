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
 * util_daxemu.c -- test memory mapping and file I/O on Device DAX
 *
 * usage: util_daxemu devdax ...
 *
 * Expected Device DAX behavior (kernel 4.11.8):
 *
 * - MAP_PRIVATE is not supported - mmap() fails with EINVAL.
 *
 * - Mapping length should be aligned to the internal device alignment,
 *   otherwise mmap() fails with EINVAL.  Same applies to offset.
 *
 *   NOTE: glibc aligns length to page boundary, so for 4K alignment
 *   mmap() would always succeed.  However, for 2M/1G alignment, it usually
 *   fails, unless len is close (less than 4K) to the internal alignment.
 *
 * - It is possible to create mapping larger than device size (len > dev_size)
 *   or (offset + len > dev_size) - mmap() succeeds, but attempt to read/write
 *   pages beyond device size results in SIGSEGV/SIGBUS.
 *
 * - msync() - fails with EINVAL (except for len == 0).
 *
 * - read(), write(), pread(), pwrite(), ftruncate(), fsync() - fail with
 *   EINVAL.
 *
 * - posix_fallocate() - fails with ENODEV.
 *
 * - lseek() - returns no error.
 */

#include "unittest.h"
#include <sys/mman.h>
#include <signal.h>
#include <setjmp.h>

#include "file.h"

#define PAGE_SIZE	4096

static ut_jmp_buf_t Jmp;
static int Last_signal;

/*
 * signal_handler -- called on SIGSEGV/SIGBUS
 */
static void
signal_handler(int sig)
{
	Last_signal = sig;
	ut_siglongjmp(Jmp);
}

/*
 * check_access -- check access to mapped memory
 */
static void
check_access(char *addr, size_t len, int prot)
{
	volatile int i;

	/* arrange to catch SEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);
	SIGACTION(SIGBUS, &v, NULL);

	char pat[PAGE_SIZE];
	char buf[PAGE_SIZE];

	for (i = 0; i < len / PAGE_SIZE; i++) {
		/* check read access */
		if (!ut_sigsetjmp(Jmp)) {
			memcpy(buf, addr + PAGE_SIZE * i, PAGE_SIZE);
			if ((prot & PROT_READ) == 0)
				UT_FATAL("memory can be read");
		} else {
			if (prot & PROT_READ)
				UT_FATAL("memory cannot be read");
		}
	}

	/* fill up mapped region with new pattern */
	memset(pat, 0xA5, PAGE_SIZE);
	for (i = 0; i < len / PAGE_SIZE; i++) {
		if (!ut_sigsetjmp(Jmp)) {
			memcpy(addr + PAGE_SIZE * i, pat, PAGE_SIZE);
			if ((prot & PROT_WRITE) == 0)
				UT_FATAL("memory can be written");
		} else {
			if (prot & PROT_WRITE)
				UT_FATAL("memory cannot be written");
		}
	}
}

/*
 * check_mapping -- check access to memory mapped file
 */
static void
check_mapping(int fd, char *addr, size_t len, int prot)
{
	check_access(addr, PAGE_SIZE, prot);
	if (len > PAGE_SIZE)
		check_access(addr + len - PAGE_SIZE, PAGE_SIZE, prot);
	munmap(addr, len);
}

/*
 * test_mmap_flags -- test supported flags
 */
static void
test_mmap_flags(int fd, size_t len, size_t align)
{
	char *ptr;

	/* SHARED */
	ptr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	UT_ASSERTeq(munmap(ptr, len), 0);

	/* PRIVATE */
	errno = 0;
	ptr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	UT_ASSERTeq(ptr, MAP_FAILED);
	UT_ASSERTeq(errno, EINVAL);
}

/*
 * test_mmap_len -- test various lengths
 */
static void
test_mmap_len(int fd, size_t len, size_t align)
{
	char *ptr;

	/* len == 0 */
	errno = 0;
	ptr = mmap(NULL, 0, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTeq(ptr, MAP_FAILED);
	UT_ASSERTeq(errno, EINVAL);

	/* len == dev_size */
	ptr = mmap(NULL, len, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, len, PROT_READ|PROT_WRITE);

	/* len > dev_size */
	ptr = mmap(NULL, len + align, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, len, PROT_READ|PROT_WRITE);

	/* no access to memory beyond device length */
	check_mapping(fd, ptr + len, align, PROT_NONE);

	/* len < dev_size */
	ptr = mmap(NULL, len - align, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, len - align, PROT_READ|PROT_WRITE);

	/* len is 4K-page aligned, but not to the internal dev alignment */
	if (align > PAGE_SIZE) {
		errno = 0;
		ptr = mmap(NULL, len - PAGE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
		UT_ASSERTeq(ptr, MAP_FAILED);
		UT_ASSERTeq(errno, EINVAL);
	}

	/* len < dev_size, unaligned */
	errno = 0;
	ptr = mmap(NULL, align + 100, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (align > PAGE_SIZE) {
		UT_ASSERTeq(errno, EINVAL);
		UT_ASSERTeq(ptr, MAP_FAILED);
	} else {
		UT_ASSERTne(ptr, MAP_FAILED);
		check_mapping(fd, ptr, align + 100, PROT_READ|PROT_WRITE);
	}

	/* len < dev_size, unaligned */
	ptr = mmap(NULL, 2 * align - 100, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, 2 * align - 100, PROT_READ|PROT_WRITE);
}

/*
 * test_mmap_offset -- test various offsets
 */
static void
test_mmap_offset(int fd, size_t len, size_t align)
{
	char *ptr;

	/* offset == align */
	ptr = mmap(NULL, len - align, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, align);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, len - align, PROT_READ|PROT_WRITE);

	/* offset + len > dev_size */
	ptr = mmap(NULL, len - align, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 2 * align);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, len - 2 * align, PROT_READ|PROT_WRITE);
	/* no access to memory beyond device length */
	check_mapping(fd, ptr + len - 2 * align, align, PROT_NONE);

	/* offset beyond file_size */
	ptr = mmap(NULL, align, PROT_READ, MAP_SHARED, fd,
		len + align);
	UT_ASSERTne(ptr, MAP_FAILED);
	/* check_mapping(fd, ptr, align, PROT_NONE); */
	munmap(ptr, align);

	/* offset is 4K-page aligned, but not to the internal dev alignment */
	if (align > PAGE_SIZE) {
		ptr = mmap(NULL, len - PAGE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, PAGE_SIZE);
		UT_ASSERTeq(ptr, MAP_FAILED);
	}

	/* unaligned offset */
	ptr = mmap(NULL, len - align, PROT_READ|PROT_WRITE,
		MAP_SHARED, fd, 100);
	UT_ASSERTeq(ptr, MAP_FAILED);
}

/*
 * test_munmap -- test mapping deletion
 */
static void
test_munmap(int fd, size_t len, size_t align)
{
	char *ptr1;
	char *ptr2;

	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* unaligned address - should fail */
	errno = 0;
	UT_ASSERTeq(munmap(ptr1 + 100, len), -1);
	UT_ASSERTeq(errno, EINVAL);
	check_mapping(fd, ptr1, len, PROT_READ|PROT_WRITE);

	/* unaligned length - should succeed */
	UT_ASSERTeq(munmap(ptr1, len - 100), 0);
	check_mapping(fd, ptr1, len, PROT_NONE);
	check_mapping(fd, ptr1 + len - 100, 100, PROT_NONE);

	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* len == 0 - should fail */
	errno = 0;
	UT_ASSERTne(munmap(ptr1, 0), 0);
	UT_ASSERTeq(errno, EINVAL);
	check_mapping(fd, ptr1, len, PROT_READ|PROT_WRITE);

	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* delete non existing mapping - should succeed */
	UT_ASSERTeq(munmap(ptr1, len), 0);

	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* partial unmap */
	UT_ASSERTeq(munmap(ptr1, align), 0);
	check_mapping(fd, ptr1, align, PROT_NONE);
	check_mapping(fd, ptr1 + align, len - align, PROT_READ|PROT_WRITE);

	/* unmap pages from two adjacent mappings */
	ptr1 = mmap(ptr1, align * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + align * 2, align * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, align * 2);
	UT_ASSERTeq(ptr2, ptr1 + align * 2);

	UT_ASSERTeq(munmap(ptr1 + align, align * 2), 0);
	check_mapping(fd, ptr1, align, PROT_READ|PROT_WRITE);
	check_mapping(fd, ptr1 + align, align * 2, PROT_NONE);
	check_mapping(fd, ptr1 + align * 3, align,
			PROT_READ|PROT_WRITE);
}

/*
 * test_msync -- test synchronizing a file with a memory map
 */
static void
test_msync(int fd, size_t len, size_t align)
{
	char *ptr1;
	char *ptr2;

	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	ptr2 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERTeq(munmap(ptr2, len), 0);

	/* len == 0 - the only case that succeeds */
	UT_ASSERTeq(msync(ptr1, 0, MS_SYNC), 0);

	/* sync the entire mapping - should fail */
	errno = 0;
	UT_ASSERTeq(msync(ptr1, len, MS_SYNC), -1);
	UT_ASSERTeq(errno, EINVAL);

	/* unaligned pointer - should fail */
	errno = 0;
	UT_ASSERTeq(msync(ptr1 + 100, len, MS_SYNC), -1);
	UT_ASSERTeq(errno, EINVAL);

	if (align > PAGE_SIZE) {
		errno = 0;
		UT_ASSERTeq(msync(ptr1 + PAGE_SIZE, align, MS_SYNC), -1);
		UT_ASSERTeq(errno, EINVAL);
	}

	/* unaligned length - should fail */
	errno = 0;
	UT_ASSERTeq(msync(ptr1, len - 100, MS_SYNC), -1);
	UT_ASSERTeq(errno, EINVAL);

	if (align > PAGE_SIZE) {
		errno = 0;
		UT_ASSERTeq(msync(ptr1, len - PAGE_SIZE, MS_SYNC), -1);
		UT_ASSERTeq(errno, EINVAL);
	}

	/* len > mapping size - should fail */
	UT_ASSERTeq(munmap(ptr1 + len / 2, len / 2), 0);
	errno = 0;
	UT_ASSERTne(msync(ptr1, len, MS_SYNC), 0);
	UT_ASSERTeq(errno, EINVAL);

	/* partial sync - should fail */
	errno = 0;
	UT_ASSERTeq(msync(ptr1 + align, align, MS_SYNC), -1);
	UT_ASSERTeq(errno, EINVAL);

	UT_ASSERTeq(munmap(ptr1, len), 0);
}

/*
 * test_mprotect -- test memory protection
 */
static void
test_mprotect(int fd, size_t len, size_t align)
{
	char *ptr1;

	/* len == 0 - should succeed */
	ptr1 = mmap(NULL, align, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, 0, PROT_READ), 0);
	check_access(ptr1, align, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, align), 0);

	/* len > mapping size - should fail */
	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTeq(munmap(ptr1 + len / 2, len / 2), 0);
	errno = 0;
	UT_ASSERTne(mprotect(ptr1, len, PROT_READ), 0);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERTeq(munmap(ptr1, len), 0);

	/* unaligned pointer - should fail */
	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	errno = 0;
	UT_ASSERTne(mprotect(ptr1 + 100, align, PROT_READ), 0);
	UT_ASSERTeq(errno, EINVAL);
	check_access(ptr1, len, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, len), 0);

	/* unaligned len - should succeed */
	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, 2 * align - 100, PROT_READ), 0);
	check_access(ptr1, align * 2, PROT_READ);
	check_access(ptr1 + align * 2, len - align * 2,
			PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, len), 0);

#if 0
	/* XXX - temporarily disabled */
	/* unaligned len - should succeed */
	if (align > PAGE_SIZE) {
		ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		UT_ASSERTne(ptr1, MAP_FAILED);
		UT_ASSERTeq(mprotect(ptr1, align + 100, PROT_READ), 0);
		/*
		 * XXX - This is the expected behavior, but it looks like
		 * mprotect() above results in no access to the entire region,
		 * even though it's properly reported in /proc/self/maps.
		 */
		check_access(ptr1, align + PAGE_SIZE, PROT_READ);
		check_access(ptr1 + align + PAGE_SIZE, len - align - PAGE_SIZE,
				PROT_READ|PROT_WRITE);
	}
#endif

	/* partial protection change (on internal alignment boundary) */
	ptr1 = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1 + align, align, PROT_READ), 0);
	UT_ASSERTeq(mprotect(ptr1 + align * 2, align, PROT_NONE), 0);
	check_access(ptr1, align, PROT_READ|PROT_WRITE);
	check_access(ptr1 + align, align, PROT_READ);
	check_access(ptr1 + align * 2, align, PROT_NONE);
	check_access(ptr1 + align * 3, len - align * 3,
			PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, len), 0);
}

/*
 * test_stat -- test stat/fstat on device dax
 */
static void
test_stat(const char *path, int fd, size_t len, size_t align)
{
	struct stat st;

	UT_ASSERTeq(stat(path, &st), 0);
	UT_ASSERT(S_ISCHR(st.st_mode));

	UT_ASSERTeq(fstat(fd, &st), 0);
	UT_ASSERT(S_ISCHR(st.st_mode));
}

/*
 * test_syscalls -- test some syscalls not suported on Device DAX
 */
static void
test_syscalls(const char *path, int fd, size_t len, size_t align)
{
	char buf[4096];

	errno = 0;
	UT_ASSERTeq(read(fd, buf, 16), -1);
	UT_ASSERTeq(errno, EINVAL);

	errno = 0;
	UT_ASSERTeq(write(fd, buf, 16), -1);
	UT_ASSERTeq(errno, EINVAL);

	errno = 0;
	UT_ASSERTeq(pread(fd, buf, 16, 4096), -1);
	UT_ASSERTeq(errno, EINVAL);

	errno = 0;
	UT_ASSERTeq(pwrite(fd, buf, 16, 4096), -1);
	UT_ASSERTeq(errno, EINVAL);

	/* XXX - no error? */
	UT_ASSERTeq(lseek(fd, align, SEEK_SET), 0);
	UT_ASSERTeq(lseek(fd, align, SEEK_CUR), 0);

	errno = 0;
	UT_ASSERTeq(fsync(fd), -1);
	UT_ASSERTeq(errno, EINVAL);

	errno = 0;
	UT_ASSERTeq(ftruncate(fd, align), -1);
	UT_ASSERTeq(errno, EINVAL);

	UT_ASSERTeq(posix_fallocate(fd, 0, len), ENODEV);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_daxemu");

	if (argc < 2)
		UT_FATAL("usage: %s file [file...]", argv[0]);

	for (int i = 1; i < argc; i++) {
		char *path = argv[i];

		UT_ASSERTeq(util_file_is_device_dax(path), 1);

		size_t len = util_file_get_size(path);
		size_t align = util_file_device_dax_alignment(path);

		UT_OUT("path %s len %zu align %zu", path, len, align);
		int fd = OPEN(path, O_RDWR);

		test_mmap_flags(fd, len, align);
		test_mmap_len(fd, len, align);
		test_mmap_offset(fd, len, align);
		test_munmap(fd, len, align);
		test_msync(fd, len, align);
		test_mprotect(fd, len, align);
		test_stat(path, fd, len, align);
		test_syscalls(path, fd, len, align);

		CLOSE(fd);
	}

	DONE(NULL);
}
