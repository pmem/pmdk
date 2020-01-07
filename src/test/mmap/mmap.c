// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */
/*
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * mmap.c -- test memory mapping routines
 *
 * This test is intended to be used for testing Windows implementation
 * of memory mapping routines - mmap(), munmap(), msync() and mprotect().
 * Those functions should provide the same functionality as their Linux
 * counterparts, at least with respect to the features that are used
 * in PMDK libraries.
 *
 * Known issues and differences between Linux and Windows implementation
 * are described in src/common/mmap_windows.c.
 */

#include "unittest.h"
#include <sys/mman.h>
#include <signal.h>
#include <setjmp.h>

#ifdef _WIN32
#define MMAP_ALIGN	((uintptr_t)65536)
#else
#define MMAP_ALIGN	((uintptr_t)4096)
#endif

#define PAGE_SIZE	4096
#define MMAP_SIZE	MMAP_ALIGN
#define FILE_SIZE	(MMAP_SIZE * 4)

#define CHECK_RO	1
#define CHECK_PRIV	2

static ut_jmp_buf_t Jmp;

/*
 * signal_handler -- called on SIGSEGV
 */
static void
signal_handler(int sig)
{
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
check_mapping(int fd, char *addr, size_t len, int prot,
	int flags, os_off_t offset)
{
	volatile int i;

	/* arrange to catch SEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	char pat[PAGE_SIZE] = { 0 };
	char buf[PAGE_SIZE];

	if ((flags & CHECK_RO) == 0 && fd != -1) {
		/* write some pattern to the file */
		memset(pat, 0x5A, PAGE_SIZE);

		for (i = 0; i < len / PAGE_SIZE; i++) {
			LSEEK(fd, offset + PAGE_SIZE * i, SEEK_SET);
			WRITE(fd, pat, PAGE_SIZE);

			LSEEK(fd, offset + PAGE_SIZE * i, SEEK_SET);
			if (READ(fd, buf, PAGE_SIZE) == PAGE_SIZE) {
				if (memcmp(pat, buf, PAGE_SIZE))
					UT_FATAL("first %d bytes do not match",
							PAGE_SIZE);
			}
		}
	}

	check_access(addr, len, prot);

	munmap(addr, len);

	/* same memcpy from above should now fail */
	for (i = 0; i < len / PAGE_SIZE; i++) {
		if (!ut_sigsetjmp(Jmp)) {
			memcpy(addr + PAGE_SIZE * i, pat, PAGE_SIZE);
			UT_FATAL("unmap failed");
		}
	}

	if (fd != -1) {
		/* expected pattern */
		if ((flags & (CHECK_PRIV | CHECK_RO)) != 0 ||
		    (prot & PROT_WRITE) == 0)
			memset(pat, 0x5A, PAGE_SIZE);
		else
			memset(pat, 0xA5, PAGE_SIZE);

		for (i = 0; i < len / PAGE_SIZE; i++) {
			LSEEK(fd, offset + PAGE_SIZE * i, SEEK_SET);
			if (READ(fd, buf, PAGE_SIZE) == PAGE_SIZE) {
				if (memcmp(pat, buf, PAGE_SIZE))
					UT_FATAL("first %d bytes do not match",
							PAGE_SIZE);
			}
		}
	}
}

/*
 * test_mmap_flags -- test supported flags
 */
static void
test_mmap_flags(int fd)
{
	char *ptr1;

	/* PRIVATE + SHARED */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_SHARED, fd, 0);
	UT_ASSERTeq(ptr1, MAP_FAILED);

	/* no PRIVATE/SHARED */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, 0, fd, 0);
	UT_ASSERTeq(ptr1, MAP_FAILED);

	/* ANON but no PRIVATE/SHARED */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_ANON, fd, 0);
	UT_ASSERTeq(ptr1, MAP_FAILED);
}

/*
 * test_mmap_len -- test various lengths and offsets
 */
static void
test_mmap_len(int fd)
{
	char *ptr;

	/* len == 0 */
	ptr = mmap(NULL, 0, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTeq(ptr, MAP_FAILED);

	/* len > file_size */
	ptr = mmap(NULL, FILE_SIZE + MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, FILE_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);
	UT_ASSERTeq(munmap(ptr + FILE_SIZE, MMAP_SIZE), 0);

	/* offset == 0 */
	ptr = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* offset == PAGE_SIZE */
	ptr = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, PAGE_SIZE);
#ifndef _WIN32
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, PAGE_SIZE);
#else
	/* XXX - on Windows, offset must be aligned to allocation granularity */
	UT_ASSERTeq(ptr, MAP_FAILED);
#endif

	/* offset == MMAP_ALIGN */
	ptr = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, MMAP_ALIGN);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, MMAP_ALIGN);

	/* unaligned offset */
	ptr = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 100);
	UT_ASSERTeq(ptr, MAP_FAILED);

	/* offset + len > file_size */
	ptr = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, MMAP_SIZE);
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, FILE_SIZE - MMAP_SIZE, PROT_READ|PROT_WRITE,
			CHECK_PRIV, MMAP_SIZE);
	UT_ASSERTeq(munmap(ptr + FILE_SIZE - MMAP_SIZE, MMAP_SIZE), 0);

	/* offset beyond file_size */
	ptr = mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, fd,
		FILE_SIZE + MMAP_SIZE);
#ifndef _WIN32
	UT_ASSERTne(ptr, MAP_FAILED);
	check_mapping(fd, ptr, MMAP_SIZE, PROT_READ, CHECK_PRIV,
		FILE_SIZE + MMAP_SIZE);
#else
	UT_ASSERTeq(ptr, MAP_FAILED);
#endif
}

/*
 * test_mmap_hint -- test hint address
 */
static void
test_mmap_hint(int fd)
{
	char *ptr1;
	char *ptr2;

	/* map entire file first to get unused address */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);

	/* now try to map a part of it at specified address */
	ptr2 = mmap(ptr1 + MMAP_ALIGN, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_ALIGN);
	check_mapping(fd, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);

	/* non-aligned hint address - should be ignored */
	ptr2 = mmap(ptr1 + 100, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERTne(ptr2, ptr1 + 100);
	check_mapping(fd, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);

	/* hint address is busy */
	ptr1 = mmap(NULL, FILE_SIZE / 2, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERT(ptr2 < ptr1 || ptr2 >= ptr1 + FILE_SIZE / 2);
	munmap(ptr1, FILE_SIZE / 2);
	check_mapping(fd, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);
}

/*
 * test_mmap_fixed -- test MAP_FIXED flag
 */
static void
test_mmap_fixed(int fd)
{
	char *ptr1;
	char *ptr2;

	/* map entire file first to get unused address */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);

	/* now try to map a part of it at specified address */
	ptr2 = mmap(ptr1 + MMAP_ALIGN, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_FIXED, fd, 0);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_ALIGN);
	check_mapping(fd, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);

	/* non-aligned hint address - should fail */
	ptr2 = mmap(ptr1 + 100, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_FIXED, fd, 0);
	UT_ASSERTeq(ptr2, MAP_FAILED);

	/* hint address is busy */
	ptr1 = mmap(NULL, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_PRIVATE, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_FIXED, fd, 0);

	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_SIZE);
	check_mapping(fd, ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);
	check_mapping(fd, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, CHECK_PRIV, 0);
}

/*
 * test_mmap_anon -- test anonymous mappings
 */
static void
test_mmap_anon(int fd)
{
	char *ptr1;
	char *ptr2;

	/* fd == -1, but no MAP_ANON - should fail */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, -1, 0);
	UT_ASSERTeq(ptr1, MAP_FAILED);

	/* fd should be ignored */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(-1, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* offset should be ignored */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, -1, MMAP_ALIGN);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(-1, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* now try to map a part of it at specified address */
	ptr2 = mmap(ptr1 + MMAP_ALIGN, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, -1, 0);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_ALIGN);
	check_mapping(-1, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* non-aligned hint address - should be ignored */
	ptr2 = mmap(ptr1 + 100, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, -1, 0);
	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERTne(ptr2, ptr1 + 100);
	check_mapping(-1, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* non-aligned hint address + MAP_FIXED - should fail */
	ptr2 = mmap(ptr1 + 100, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED|MAP_FIXED, -1, 0);
	UT_ASSERTeq(ptr2, MAP_FAILED);

	/* hint address is busy */
	ptr1 = mmap(NULL, FILE_SIZE / 2, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, -1, 0);
	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERT(ptr2 < ptr1 || ptr2 >= ptr1 + FILE_SIZE / 2);
	munmap(ptr1, FILE_SIZE / 2);
	check_mapping(-1, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* hint address is busy + MAP_FIXED */
	ptr1 = mmap(NULL, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_SHARED|MAP_FIXED, -1, 0);

	UT_ASSERTne(ptr2, MAP_FAILED);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_SIZE);
	check_mapping(-1, ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);
	check_mapping(-1, ptr2, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);
}

/*
 * test_mmap_prot -- test R/W protection
 */
static void
test_mmap_prot(int fd, int fd_ro)
{
	char *ptr1;

	/* read/write */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* read/write on file opened in read-only mode - should fail */
	errno = 0;
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd_ro, 0);
	UT_ASSERTeq(ptr1, MAP_FAILED);
	UT_ASSERTeq(errno, EACCES);

	/* read-only */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ, 0, 0);

	/* read-only on file opened in read-only mode - should succeed */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ, MAP_SHARED, fd_ro, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd_ro, ptr1, FILE_SIZE, PROT_READ, CHECK_RO, 0);

	/* no access */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_NONE, MAP_SHARED, fd, 0);
#ifndef _WIN32
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_NONE, 0, 0);
#else
	/* XXX - PROT_NONE not supported yet */
	UT_ASSERTeq(ptr1, MAP_FAILED);
#endif

	/* no access on read-only file */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_NONE, MAP_SHARED, fd_ro, 0);
#ifndef _WIN32
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd_ro, ptr1, FILE_SIZE, PROT_NONE, CHECK_RO, 0);
#else
	/* XXX - PROT_NONE not supported yet */
	UT_ASSERTeq(ptr1, MAP_FAILED);
#endif
}

/*
 * test_mmap_prot_anon -- test R/W protection on anonymous mappings
 */
static void
test_mmap_prot_anon(void)
{
	char *ptr1;

	/* read/write */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(-1, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* read-only */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ, MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(-1, ptr1, FILE_SIZE, PROT_READ, 0, 0);

	/* no access */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_NONE, MAP_SHARED|MAP_ANON, -1, 0);
#ifndef _WIN32
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(-1, ptr1, FILE_SIZE, PROT_NONE, 0, 0);
#else
	/* XXX - PROT_NONE not supported yet */
	UT_ASSERTeq(ptr1, MAP_FAILED);
#endif
}

/*
 * test_mmap_shared -- test shared mappings
 */
static void
test_mmap_shared(int fd)
{
	char *ptr1;

	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);
}

/*
 * test_munmap -- test mapping deletion
 */
static void
test_munmap(int fd)
{
	char *ptr1;
	char *ptr2;

	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* unaligned address - should fail */
	errno = 0;
	UT_ASSERTeq(munmap(ptr1 + 100, FILE_SIZE), -1);
	UT_ASSERTeq(errno, EINVAL);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	/* unaligned length - should succeed */
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE - 100), 0);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_NONE, 0, 0);
	check_mapping(fd, ptr1 + FILE_SIZE - 100, 100, PROT_NONE, 0, 0);

	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* len == 0 - should fail */
	errno = 0;
	UT_ASSERTne(munmap(ptr1, 0), 0);
	UT_ASSERTeq(errno, EINVAL);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_READ|PROT_WRITE, 0, 0);

	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* delete entire mapping (len > file_size) */
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE + MMAP_SIZE), 0);
	check_mapping(fd, ptr1, FILE_SIZE, PROT_NONE, 0, 0);

	/* delete non existing mapping - should succeed */
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	/* partial unmap */
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);
	check_mapping(fd, ptr1, MMAP_SIZE, PROT_NONE, 0, 0);
	check_mapping(fd, ptr1 + MMAP_SIZE, FILE_SIZE - MMAP_SIZE,
			PROT_READ|PROT_WRITE, 0, MMAP_SIZE);

	/* unmap pages from two adjacent mappings */
	ptr1 = mmap(ptr1, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE * 2, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, MMAP_SIZE * 2);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_SIZE * 2);

	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE, MMAP_SIZE * 2), 0);
	check_mapping(fd, ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE, 0, 0);
	check_mapping(fd, ptr1 + MMAP_SIZE, MMAP_SIZE * 2,
			PROT_NONE, 0, MMAP_SIZE);
	check_mapping(fd, ptr1 + MMAP_SIZE * 3, MMAP_SIZE,
			PROT_READ|PROT_WRITE, 0, MMAP_SIZE * 3);
}

#define MS_ALL (MS_SYNC|MS_ASYNC|MS_INVALIDATE)

/*
 * test_msync -- test synchronizing a file with a memory map
 */
static void
test_msync(int fd)
{
	char *ptr1;
	char *ptr2;

	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	ptr2 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(munmap(ptr2, FILE_SIZE), 0);

	/* unknown flag - should fail */
	errno = 0;
	UT_ASSERTne(msync(ptr1, MMAP_SIZE, MS_ALL + 1), 0);
	UT_ASSERTeq(errno, EINVAL);

	/* SYNC + ASYNC - should fail */
	errno = 0;
	UT_ASSERTne(msync(ptr1, MMAP_SIZE, MS_SYNC|MS_ASYNC), 0);
	UT_ASSERTeq(errno, EINVAL);

	/* no SYNC, nor ASYNC - should fail according to POSIX... */
	errno = 0;
#ifndef _WIN32
	/* ... but it is allowed on Linux */
	UT_ASSERTeq(msync(ptr1, MMAP_SIZE, 0), 0);
	UT_ASSERTeq(errno, 0);
#else
	UT_ASSERTne(msync(ptr1, MMAP_SIZE, 0), 0);
	UT_ASSERTeq(errno, EINVAL);
#endif

	/* len == 0 - should succeed */
	UT_ASSERTeq(msync(ptr1, 0, MS_SYNC), 0);

	/* len == SIZE_MAX - should fail */
	errno = 0;
#ifndef _WIN32
	/* ... but it is allowed on Linux */
	UT_ASSERTeq(msync(ptr1, SIZE_MAX, MS_SYNC), 0);
	UT_ASSERTeq(errno, 0);
#else
	UT_ASSERTne(msync(ptr1, SIZE_MAX, MS_SYNC), 0);
	UT_ASSERTeq(errno, ENOMEM);
#endif

	/* unaligned pointer - should fail */
	errno = 0;
	UT_ASSERTne(msync(ptr1 + 100, FILE_SIZE, MS_SYNC), 0);
	UT_ASSERTeq(errno, EINVAL);

	/* invalid pointer - should fail */
	UT_ASSERTne(msync(ptr2, FILE_SIZE, MS_SYNC), 0);

	/* unaligned length - should succeed */
	UT_ASSERTeq(msync(ptr1, FILE_SIZE - 100, MS_SYNC), 0);

	/* len > mapping size - should fail */
	UT_ASSERTeq(munmap(ptr1 + FILE_SIZE / 2, FILE_SIZE / 2), 0);
	errno = 0;
	UT_ASSERTne(msync(ptr1, FILE_SIZE, MS_SYNC), 0);
	UT_ASSERTeq(errno, ENOMEM);

	/* partial sync */
	UT_ASSERTeq(msync(ptr1 + PAGE_SIZE, MMAP_SIZE, MS_SYNC), 0);

	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* range includes invalid addresses - should fail */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE, MMAP_SIZE), 0);
	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE * 3, MMAP_SIZE), 0);
	errno = 0;
	UT_ASSERTne(msync(ptr1, FILE_SIZE, MS_SYNC), 0);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* synchronize two adjacent mappings */
	ptr1 = mmap(ptr1, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE * 2, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, MMAP_SIZE * 2);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_SIZE * 2);
	UT_ASSERTeq(msync(ptr1 + MMAP_SIZE, MMAP_SIZE * 2, MS_SYNC), 0);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE * 4), 0);

	/* anonymous mapping */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(msync(ptr1, FILE_SIZE, MS_SYNC), 0);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);
}

#define PROT_ALL (PROT_READ|PROT_WRITE|PROT_EXEC)

/*
 * test_mprotect -- test memory protection
 */
static void
test_mprotect(int fd, int fd_ro)
{
	char *ptr1;
	char *ptr2;

	/* unknown PROT flag - should succeed */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_ALL + 1), 0);
	check_access(ptr1, MMAP_SIZE, PROT_NONE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* len == 0 - should succeed */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, 0, PROT_READ), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* len > mapping size - should fail */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTeq(munmap(ptr1 + FILE_SIZE / 2, FILE_SIZE / 2), 0);
	errno = 0;
	UT_ASSERTne(mprotect(ptr1, FILE_SIZE, PROT_READ), 0);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* change protection: R/O => R/W */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
#ifndef _WIN32
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);
#else
	/* XXX - not supported yet */
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);
#endif

	/* change protection; R/W => R/O */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_READ), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* change protection; R/W => none */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_NONE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_NONE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* unaligned pointer - should fail */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	errno = 0;
	UT_ASSERTne(mprotect(ptr1 + 100, MMAP_SIZE, PROT_READ), 0);
	UT_ASSERTeq(errno, EINVAL);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* invalid pointer - should fail */
	errno = 0;
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE, PROT_READ), 0);
	UT_ASSERTeq(errno, ENOMEM);

	/* unaligned len - should succeed */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, PAGE_SIZE + 100, PROT_READ), 0);
	check_access(ptr1, PAGE_SIZE * 2, PROT_READ);
	check_access(ptr1 + PAGE_SIZE * 2, FILE_SIZE - PAGE_SIZE * 2,
			PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* partial protection change (on page boundary) */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1 + PAGE_SIZE, PAGE_SIZE, PROT_READ), 0);
	UT_ASSERTeq(mprotect(ptr1 + PAGE_SIZE * 2, PAGE_SIZE, PROT_NONE), 0);
	check_access(ptr1, PAGE_SIZE, PROT_READ|PROT_WRITE);
	check_access(ptr1 + PAGE_SIZE, PAGE_SIZE, PROT_READ);
	check_access(ptr1 + PAGE_SIZE * 2, PAGE_SIZE, PROT_NONE);
	check_access(ptr1 + PAGE_SIZE * 3, FILE_SIZE - PAGE_SIZE * 3,
			PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* range includes invalid addresses - should fail */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE, MMAP_SIZE), 0);
	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE * 3, MMAP_SIZE), 0);
	check_access(ptr1 + MMAP_SIZE, MMAP_SIZE, PROT_NONE);
	check_access(ptr1 + MMAP_SIZE * 3, MMAP_SIZE, PROT_NONE);

	errno = 0;
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE * 4, PROT_READ), 0);
	UT_ASSERTeq(errno, ENOMEM);
#ifndef _WIN32
	/* protection changed for all the pages up to the first invalid */
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	check_access(ptr1 + MMAP_SIZE * 2, MMAP_SIZE, PROT_READ|PROT_WRITE);
#else
	/* XXX - protection changed for all the valid pages */
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	check_access(ptr1 + MMAP_SIZE * 2, MMAP_SIZE, PROT_READ);
#endif
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* change protection on two adjacent mappings */
	ptr1 = mmap(ptr1, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE * 2, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, MMAP_SIZE * 2);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_SIZE * 2);
	UT_ASSERTeq(mprotect(ptr1 + MMAP_SIZE, MMAP_SIZE * 2, PROT_NONE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	check_access(ptr1 + MMAP_SIZE, MMAP_SIZE * 2, PROT_NONE);
	check_access(ptr1 + MMAP_SIZE * 3, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE * 4), 0);

	/* change protection to R/W on file opened in read-only mode */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, fd_ro, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	errno = 0;
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE), 0);
	UT_ASSERTeq(errno, EACCES);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);
}

/*
 * test_mprotect_anon -- test memory protection on anonymous mappings
 */
static void
test_mprotect_anon(void)
{
	char *ptr1;
	char *ptr2;

	/* unknown PROT flag - should succeed */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_ALL + 1), 0);
	check_access(ptr1, MMAP_SIZE, PROT_NONE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* len == 0 - should succeed */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, 0, PROT_READ), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* change protection: R/O => R/W */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ, MAP_PRIVATE|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
#ifndef _WIN32
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);
#else
	/* XXX - not supported yet */
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);
#endif

	/* change protection; R/W => R/O */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_READ), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* change protection; R/W => none */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, MMAP_SIZE, PROT_NONE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_NONE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* unaligned pointer - should fail */
	ptr1 = mmap(NULL, MMAP_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	errno = 0;
	UT_ASSERTne(mprotect(ptr1 + 100, MMAP_SIZE, PROT_READ), 0);
	UT_ASSERTeq(errno, EINVAL);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE), 0);

	/* invalid pointer - should fail */
	errno = 0;
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE, PROT_READ), 0);
	UT_ASSERTeq(errno, ENOMEM);

	/* unaligned len - should succeed */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1, PAGE_SIZE + 100, PROT_READ), 0);
	check_access(ptr1, PAGE_SIZE * 2, PROT_READ);
	check_access(ptr1 + PAGE_SIZE * 2, FILE_SIZE - PAGE_SIZE * 2,
			PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* partial protection change (on page boundary) */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(mprotect(ptr1 + PAGE_SIZE, PAGE_SIZE, PROT_READ), 0);
	UT_ASSERTeq(mprotect(ptr1 + PAGE_SIZE * 2, PAGE_SIZE, PROT_NONE), 0);
	check_access(ptr1, PAGE_SIZE, PROT_READ|PROT_WRITE);
	check_access(ptr1 + PAGE_SIZE, PAGE_SIZE, PROT_READ);
	check_access(ptr1 + PAGE_SIZE * 2, PAGE_SIZE, PROT_NONE);
	check_access(ptr1 + PAGE_SIZE * 3, FILE_SIZE - PAGE_SIZE * 3,
			PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* range includes invalid addresses - should fail */
	ptr1 = mmap(NULL, FILE_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE, MMAP_SIZE), 0);
	UT_ASSERTeq(munmap(ptr1 + MMAP_SIZE * 3, MMAP_SIZE), 0);
	check_access(ptr1 + MMAP_SIZE, MMAP_SIZE, PROT_NONE);
	check_access(ptr1 + MMAP_SIZE * 3, MMAP_SIZE, PROT_NONE);

	errno = 0;
	UT_ASSERTne(mprotect(ptr1, MMAP_SIZE * 4, PROT_READ), 0);
	UT_ASSERTeq(errno, ENOMEM);
#ifndef _WIN32
	/* protection changed for all the pages up to the first invalid */
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	check_access(ptr1 + MMAP_SIZE * 2, MMAP_SIZE, PROT_READ|PROT_WRITE);
#else
	/* XXX - protection changed for all the valid pages */
	check_access(ptr1, MMAP_SIZE, PROT_READ);
	check_access(ptr1 + MMAP_SIZE * 2, MMAP_SIZE, PROT_READ);
#endif
	UT_ASSERTeq(munmap(ptr1, FILE_SIZE), 0);

	/* change protection on two adjacent mappings */
	ptr1 = mmap(ptr1, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);
	ptr2 = mmap(ptr1 + MMAP_SIZE * 2, MMAP_SIZE * 2, PROT_READ|PROT_WRITE,
			MAP_SHARED|MAP_ANON, -1, MMAP_SIZE * 2);
	UT_ASSERTeq(ptr2, ptr1 + MMAP_SIZE * 2);
	UT_ASSERTeq(mprotect(ptr1 + MMAP_SIZE, MMAP_SIZE * 2, PROT_NONE), 0);
	check_access(ptr1, MMAP_SIZE, PROT_READ|PROT_WRITE);
	check_access(ptr1 + MMAP_SIZE, MMAP_SIZE * 2, PROT_NONE);
	check_access(ptr1 + MMAP_SIZE * 3, MMAP_SIZE, PROT_READ|PROT_WRITE);
	UT_ASSERTeq(munmap(ptr1, MMAP_SIZE * 4), 0);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "mmap");

	if (argc !=  2)
		UT_FATAL("usage: %s file", argv[0]);

	int fd = OPEN(argv[1], O_RDWR);
	int fd_ro = OPEN(argv[1], O_RDONLY);

	POSIX_FALLOCATE(fd, 0, FILE_SIZE);

	test_mmap_flags(fd);
	test_mmap_len(fd);
	test_mmap_hint(fd);
	test_mmap_fixed(fd);
	test_mmap_anon(fd);
	test_mmap_shared(fd);
	test_mmap_prot(fd, fd_ro);
	test_mmap_prot_anon();
	test_munmap(fd);
	test_msync(fd);
	test_mprotect(fd, fd_ro);
	test_mprotect_anon();

	CLOSE(fd_ro);
	CLOSE(fd);

	DONE(NULL);
}
