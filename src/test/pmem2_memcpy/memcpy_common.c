/*
 * Copyright 2020, Intel Corporation
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
 * memcpy_common.c -- common part for tests doing a persistent memcpy
 */

#include "unittest.h"
#include "memcpy_common.h"

/*
 * swap_mappings - given to mmapped regions swap them.
 *
 * Try swapping src and dest by unmapping src, mapping a new dest with
 * the original src address as a hint. If successful, unmap original dest.
 * Map a new src with the original dest as a hint.
 */
void
swap_mappings(char **dest, char **src, size_t size, int fd)
{
	char *d = *dest;
	char *s = *src;
	char *td, *ts;

	MUNMAP(*src, size);

	/* mmap destination using src addr as hint */
	td = MMAP(s, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	MUNMAP(*dest, size);
	*dest = td;

	/* mmap src using original destination addr as a hint */
	ts = MMAP(d, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,
		-1, 0);
	*src = ts;
}

/*
 * do_memcpy: Worker function for memcpy
 *
 * Always work within the boundary of bytes. Fill in 1/2 of the src
 * memory with the pattern we want to write. This allows us to check
 * that we did not overwrite anything we were not supposed to in the
 * dest.  Use the non pmem version of the memset/memcpy commands
 * so as not to introduce any possible side affects.
 */

void
do_memcpy(int fd, char *dest, int dest_off, char *src, int src_off,
    size_t bytes, const char *file_name, memcpy_fn fn, unsigned flags,
    union persist p)
{
	void *ret;
	char *buf = MALLOC(bytes);

	memset(buf, 0, bytes);
	memset(dest, 0, bytes);
	memset(src, 0, bytes);
	do_persist(p, src, bytes);

	memset(src, 0x5A, bytes / 4);
	do_persist(p, src, bytes / 4);
	memset(src + bytes / 4, 0x46, bytes / 4);
	do_persist(p, src + bytes / 4, bytes / 4);

	/* dest == src */
	ret = fn(dest + dest_off, dest + dest_off, bytes / 2, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	/* len == 0 */
	ret = fn(dest + dest_off, src, 0, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	ret = fn(dest + dest_off, src + src_off, bytes / 2, flags);
	UT_ASSERTeq(ret, dest + dest_off);

	/* memcmp will validate that what I expect in memory. */
	if (memcmp(src + src_off, dest + dest_off, bytes / 2))
		UT_FATAL("%s: first %zu bytes do not match",
			file_name, bytes / 2);

	/* Now validate the contents of the file */
	LSEEK(fd, (os_off_t)dest_off, SEEK_SET);
	if (READ(fd, buf, bytes / 2) == bytes / 2) {
		if (memcmp(src + src_off, buf, bytes / 2))
			UT_FATAL("%s: first %zu bytes do not match",
				file_name, bytes / 2);
	}

	FREE(buf);
}

unsigned Flags[] = {
		0,
		PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_NONTEMPORAL,
		PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_WC,
		PMEM_F_MEM_WB,
		PMEM_F_MEM_NOFLUSH,
		/* all possible flags */
		PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH |
			PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL |
			PMEM_F_MEM_WC | PMEM_F_MEM_WB,
};
