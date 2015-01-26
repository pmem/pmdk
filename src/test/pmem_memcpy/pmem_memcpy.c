/*
 * Copyright (c) 2015, Intel Corporation
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
 * pmem_memcpy.c -- unit test for doing a memcpy
 *
 * usage: pmem_memcpy_persist < dest addr, src addr, length>
 *
 */

#include "unittest.h"
#include "libpmem.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * swap_mappings - given to mmapped regions swap them.
 *
 * Try swapping src and dest by unmapping src, mapping a new dest with
 * the original src address as a hint. If successful, unmap original dest.
 * Map a new src with the original dest as a hint.
 */
int
swap_mappings(void **dest, void **src, size_t size, int fd)
{

	void *d = *dest;
	void *s = *src;
	void *td, *ts;

	munmap(*src, size);

	/* mmap destination using src addr as hint */
	td = mmap(s, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (td == (void *) -1) {
		/*
		 * Original src is unmapped at this point. If there is a failure
		 * then clean up last successful mapping and return error.
		 */
		MUNMAP(*dest, size);
		OUT("could not mmap dest file, err: %s", strerror(errno));
		return -1;
	}

	munmap(*dest, size);
	*dest = td;

	/* mmap src using original destination addr as a hint */
	ts = mmap(d, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
		-1, 0);
	if (ts == (void *) -1) {
		/*
		 * Original src and dest are unmapped at this point. If
		 * there is a failure then clean up last successful mapping
		 * and return error.
		 */
		MUNMAP(td, size);
		OUT("could not mmap source file, err: %s", strerror(errno));
		return -1;
	}
	*src = ts;

	/*
	 * Original mappings have been removed. New mappings are assigned.
	 */
	return 0;
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
do_memcpy(int fd, void *dest, int dest_off, void *src, int src_off,
    size_t bytes, char *file_name)
{
	char buf[bytes];

	memset(buf, 0, bytes);
	memset(dest, 0, bytes);
	memset(src, 0, bytes);

	memset(src, 0x5A, bytes/4);
	memset(src + bytes/4, 0x46, bytes/4);
	pmem_memcpy_persist(dest + dest_off, src + src_off, bytes/2);

	/* memcmp will validate that what I expect in memory. */
	if (memcmp(src + src_off, dest + dest_off, bytes/2))
		OUT("%s: first %zu bytes do not match",
			file_name, bytes/2);

	/* Now validate the contents of the file */
	LSEEK(fd, (off_t)dest_off, SEEK_SET);
	if (READ(fd, buf, bytes/2) == bytes/2) {
		if (memcmp(src + src_off, buf, bytes/2))
			OUT("%s: first %zu bytes do not match",
				file_name, bytes/2);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	void *dest;
	void *src;
	struct stat stbuf;

	START(argc, argv, "pmem_memcpy");

	if (argc != 5)
		FATAL("usage: %s dest offset, src offset, num bytes", argv[0]);

	fd = OPEN(argv[1], O_RDWR);
	int dest_off = atoi(argv[2]);
	int src_off = atoi(argv[3]);
	size_t bytes = strtoul(argv[4], NULL, 0);

	FSTAT(fd, &stbuf);

	/* src > dst */
	dest = pmem_map(fd);

	if (dest == (void *) -1) {
		OUT("Could not map file %s, err: %s", argv[1], strerror(errno));
		goto err;
	}

	src = mmap(dest + stbuf.st_size, stbuf.st_size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (src == (void *) -1) {
		OUT("Could not map anonymous memory, err: %s", strerror(errno));
		goto err;
	}
	/*
	 * Its very unlikely that src would not be > dest. pmem_map
	 * chooses the first unused address >= 1TB, large
	 * enough to hold the give range, and 1GB aligned. The
	 * next call to mmap should be > than this. However,
	 * in unlikely case it happens try to force this one more time then
	 * exit.
	 */
	if (src <= dest) {
		if ((swap_mappings(&dest, &src, stbuf.st_size, fd)) != 0) {
			MUNMAP(dest, stbuf.st_size);
			MUNMAP(src, stbuf.st_size);
			goto err;
		}
		if (src <= dest) {
			MUNMAP(dest, stbuf.st_size);
			MUNMAP(src, stbuf.st_size);
			OUT("cannot map files in memory order");
			goto err;
		}
	}

	memset(dest, 0, (2 * bytes));
	memset(src, 0, (2 * bytes));

	do_memcpy(fd, dest, dest_off, src, src_off, bytes, argv[1]);

	/* dest > src */

	/* Simply swap mappings to get dest > src */
	if ((swap_mappings(&dest, &src, stbuf.st_size, fd)) != 0)
		goto err;

	if (dest <= src) {
		OUT("cannot map files in memory order");
		goto err;
	}

	do_memcpy(fd, dest, dest_off, src, src_off, bytes, argv[1]);
	MUNMAP(dest, stbuf.st_size);
	MUNMAP(src, stbuf.st_size);

err:
	CLOSE(fd);

	DONE(NULL);
}
