/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * blk_nblock.c -- unit test for pmemblk_nblock()
 *
 * usage: blk_nblock bsize:file...
 *
 */
#define	_GNU_SOURCE
#include "unittest.h"
#include <dlfcn.h>

#define	MMAP_MAX_SIZE (1024L * 1024L * 1024L)

static void *mapped_addr;

/*
 * mmap -- interpose on libc mmap()
 *
 * This catches mmap and reduces mapped size up to 1GB.
 * Code under test doesn’t actually write outside first 1GB of mapped region.
 * This limit is necessary to work on machines without overcommit.
 */
void *
mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	static void *(*mmap_ptr)(void *addr, size_t len, int prot, int flags,
		int fildes, off_t off);
	if (mmap_ptr == NULL)
		mmap_ptr = dlsym(RTLD_NEXT, "mmap");

	if (len < MMAP_MAX_SIZE)
		return mmap_ptr(addr, len, prot, flags, fildes, off);

	len = MMAP_MAX_SIZE;
	ASSERTeq(mapped_addr, NULL);
	mapped_addr = mmap_ptr(addr, len, prot, flags, fildes, off);
	return mapped_addr;
}

/*
 * munmap -- interpose on libc munmap()
 *
 * This catches munmap and changes length to size used inside interpose mmap.
 */
int
munmap(void *addr, size_t len)
{
	static int(*munmap_ptr)(void *addr, size_t len);
	if (munmap_ptr == NULL)
		munmap_ptr = dlsym(RTLD_NEXT, "munmap");

	if (addr == mapped_addr) {
		len = MMAP_MAX_SIZE;
		mapped_addr = NULL;
	}

	return munmap_ptr(addr, len);
}

/*
 * mprotect -- interpose on libc mprotect()
 *
 * This catches mprotect and adjusts length to max_addr
 * set inside interposed mmap.
 */
int
mprotect(void *addr, size_t len, int prot)
{
	static int (*mprotect_ptr)(void *addr, size_t len, int prot);
	if (mprotect_ptr == NULL)
		mprotect_ptr = dlsym(RTLD_NEXT, "mprotect");

	if (mapped_addr != NULL) {
		uintptr_t addr_int = (uintptr_t)addr;
		uintptr_t min_addr = (uintptr_t)mapped_addr;
		uintptr_t max_addr = (uintptr_t)(mapped_addr) + MMAP_MAX_SIZE;

		/* unit tests are not supposed to write beyond the 1GB range */
		if (addr_int >= min_addr && addr_int <= max_addr &&
				addr_int + len > max_addr) {
			ASSERTeq(prot & PROT_WRITE, 0);
			len = max_addr - addr_int;
		}
	}

	return mprotect_ptr(addr, len, prot);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_nblock");

	if (argc < 2)
		FATAL("usage: %s bsize:file...", argv[0]);

	/* map each file argument with the given map type */
	for (int arg = 1; arg < argc; arg++) {
		char *fname;
		size_t bsize = strtoul(argv[arg], &fname, 0);
		if (*fname != ':')
			FATAL("usage: %s bsize:file...", argv[0]);
		fname++;

		PMEMblkpool *handle;
		if ((handle = pmemblk_pool_open(fname, bsize)) == NULL)
			OUT("!%s: pmemblk_pool_open", fname);
		else {
			OUT("%s: block size %zu usable blocks: %zu",
					fname, bsize, pmemblk_nblock(handle));
			pmemblk_pool_close(handle);
			int result = pmemblk_pool_check(fname);
			if (result < 0)
				OUT("!%s: pmemblk_pool_check", fname);
			else if (result == 0)
				OUT("%s: pmemblk_pool_check: not consistent",
						fname);
		}
	}

	DONE(NULL);
}
