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
 * pmem_memcpy.c -- unit test for doing a memcpy
 *
 * usage: pmem_memcpy < dest addr, src addr, length>
 *
 */

#include "unittest.h"
#include "libpmem.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

void do_memcpy(int fd, void *dest, int dest_off, void *src, int src_off,
    size_t bytes, char *file_name)
{


	char buf[bytes];

	memset(buf, 0, bytes);

	memset(dest, 0, bytes);
	memset(src, 0, bytes);

	memset(src, 0x5A, bytes/4);
	memset(src + bytes/4, 0x46, bytes/4);
	pmem_memcpy(dest + dest_off, src + src_off, bytes/2);

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

	/* Copy backward, src > dest */
	dest = pmem_map(fd);
	if (dest == NULL) {
		OUT("Could not mmap %s: \n", argv[1]);
		goto err;
	}
	src = ANON_MMAP(bytes);

	do_memcpy(fd, dest, dest_off, src, src_off, bytes, argv[1]);
	MUNMAP(src, bytes);
	MUNMAP(dest, stbuf.st_size);

	/* Copy forward, dest > src */
	src =  ANON_MMAP(bytes);

	dest = pmem_map(fd);
	if (dest == NULL) {
		OUT("Could not mmap %s: \n", argv[1]);
		goto err;
	}
	do_memcpy(fd, dest, dest_off, src, src_off, bytes, argv[1]);
	MUNMAP(src, 4096);
	MUNMAP(dest, stbuf.st_size);

err:
	CLOSE(fd);

	DONE(NULL);
}
