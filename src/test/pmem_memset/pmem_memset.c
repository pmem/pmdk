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
 * pmem_memset.c -- unit test for doing a memset
 *
 * usage: pmem_memset <dest addr, int c, num bytes>
 *
 */

#include "unittest.h"
#include "libpmem.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
	int fd;
	struct stat stbuf;
	void *dest;

	START(argc, argv, "pmem_memset");

	if (argc != 4)
		FATAL("usage: %s dest off, num bytes", argv[0]);

	fd = OPEN(argv[1], O_RDWR);
	int dest_off = atoi(argv[2]);
	size_t bytes = strtoul(argv[3], NULL, 0);

	char buf[bytes];

	FSTAT(fd, &stbuf);

	dest = pmem_map(fd);
	if (dest == NULL) {
		OUT("Could not mmap %s: \n", argv[1]);
		goto err;
	}

	pmem_memset(dest + dest_off, 0x5A, bytes/2);
	pmem_memset(dest + dest_off  + (bytes/2), 0x46, (bytes/2 - dest_off));

	LSEEK(fd, (off_t)0, SEEK_SET);
	if (READ(fd, buf, bytes) == bytes) {
		if (memcmp(buf, dest, bytes))
			OUT("%s: first %zu bytes do not match",
				argv[1], bytes);
	}
	MUNMAP(dest, stbuf.st_size);

err:

	CLOSE(fd);

	DONE(NULL);
}
