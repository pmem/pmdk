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
 * pmem_memmove.c -- unit test for doing a memmove
 *
 * usage: pmem_memmove <dest addr, src addr, length>
 *
 */

#include "unittest.h"
#include "libpmem.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

/*
 * do_memmove: Worker function for memove.
 *
 * Always work within the boundary of bytes. Fill in 1/2 of the src
 * memory with the pattern we want to write. This allows us to check
 * that we did not overwrite anything we were not supposed to in the
 * dest.  Use the non pmem version of the memset/memcpy commands
 * so as not to introduce any possible side affects.
 */
void
do_memmove(int fd, void *dest, void *src, char *file_name, off_t dest_off,
	off_t src_off, off_t off, off_t bytes)
{

	void *src1 = malloc(bytes);
	void *buf = malloc(bytes);

	memset(buf, 0, bytes);
	memset(src1, 0, bytes);
	memset(src + src_off, 0x5A, bytes/4);
	memset(src + src_off + bytes/4, 0x54, bytes/4);

	/*
	 * A side affect of the memmove call is that
	 * src contents will be changed in the case of overlapping
	 * addresses.
	 */

	memcpy(src1 + src_off, src + src_off, bytes/2);
	pmem_memmove(dest + dest_off, src + src_off, bytes/2);

	/* memcmp will validate that what I expect in memory. */
	if (memcmp(src1 + src_off, dest + dest_off, bytes/2))
		OUT("%s: %zu bytes do not match with memcmp",
			file_name, bytes/2);

	/*
	 * This is a special case. An overlapping dest means that
	 * src is a pointer to the file, and destination is src + dest_off +
	 * overlap. This is the basis for the comparison.
	 */
	if (dest > src && off != 0) {
		LSEEK(fd, (off_t)dest_off + off, SEEK_SET);
		if (READ(fd, buf, bytes/2) == bytes/2) {
			if (memcmp(src1 + src_off, buf, bytes/2))
				OUT("%s: first %zu bytes do not match",
					file_name, bytes/2);
		}
	} else {
		LSEEK(fd, (off_t)dest_off, SEEK_SET);
		if (READ(fd, buf, bytes/2) == bytes/2) {
			if (memcmp(src1 + src_off, buf, bytes/2))
				OUT("%s: first %zu bytes do not match",
					file_name, bytes/2);
		}
	}
}
int
main(int argc, char *argv[])
{
	int fd;
	struct stat stbuf;
	void *dest;
	void *src;
	off_t dest_off = 0;
	off_t src_off = 0;
	off_t bytes = 0;
	int who = 0;
	off_t overlap = 0;

	START(argc, argv, "pmem_memmove");

	fd = OPEN(argv[1], O_RDWR);
	FSTAT(fd, &stbuf);

	for (int arg = 2; arg < argc; arg++) {
		if (strchr("dsboS",
		    argv[arg][0]) == NULL || argv[arg][1] != ':')
			FATAL("op must be d: or s: or b: or o: or S:");
		off_t val = strtoul(&argv[arg][2], NULL, 0);

		switch (argv[arg][0]) {
		case 'd':
			if (val <= 0)
				FATAL("Invalid value for destination offset");
			dest_off = val;
			break;
		case 's':
			if (val <= 0)
				FATAL("Invalid value for source offset");
			src_off = val;
			break;
		case 'b':
			if (val <= 0)
				FATAL("Invalid value for bytes");
			bytes = val;
			break;
		case 'o':
			who = (int)val;
			break;
		case 'S':
			overlap = val;
			break;
		}
	}

	if (who == 0 && overlap != 0)
		FATAL("Invalid overlap and source");

	/*
	 * For overlap the src and dest must be created differently.
	 */

	if (who == 0) {
		/* src > dest */
		dest = pmem_map(fd);
		if (dest == NULL) {
			OUT("Could not mmap %s: \n", argv[1]);
			goto err;
		}

		src = ANON_MMAP(bytes);
		memset(dest, 0, bytes);
		memset(src, 0, bytes);
		do_memmove(fd, dest, src, argv[1], dest_off, src_off,
			0, bytes);

		MUNMAP(dest, stbuf.st_size);
		MUNMAP(src, bytes);

		/* dest > src */
		src =  ANON_MMAP(bytes);
		dest = pmem_map(fd);
		if (dest == NULL) {
			OUT("Could not mmap %s: \n", argv[1]);
			goto err;
		}

		memset(dest, 0, bytes);
		memset(src, 0, bytes);

		do_memmove(fd, dest, src, argv[1], dest_off, src_off, 0,
			bytes);
		MUNMAP(dest, stbuf.st_size);
		MUNMAP(src, bytes);
	} else {
		if (who == 1) {
			/* src overlap with dest */
			dest = pmem_map(fd);
			if (dest == NULL) {
				OUT("Could not mmap %s: \n", argv[1]);
				goto err;
			}
			src = dest + overlap;
			memset(dest, 0, bytes);
			do_memmove(fd, dest, src, argv[1], dest_off, src_off,
				overlap, bytes);
			MUNMAP(dest, stbuf.st_size);
		}
		if (who == 2) {
			/* dest overlap with src */
			dest = pmem_map(fd);
			if (dest == NULL) {
				OUT("Could not mmap %s: \n", argv[1]);
				goto err;
			}
			src = dest;
			dest = src + overlap;
			memset(src, 0, bytes);
			do_memmove(fd, dest, src, argv[1], dest_off, src_off,
				overlap, bytes);
			MUNMAP(src, stbuf.st_size);
		}
	}

err:
	CLOSE(fd);

	DONE(NULL);
}
