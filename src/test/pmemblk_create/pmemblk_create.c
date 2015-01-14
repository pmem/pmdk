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
 * pmemblk_create.c -- unit test for creating a block memory pool
 *
 * usage: pmemblk_create path bsize
 */

#include "unittest.h"

#define	SIZEOF_TESTFILE	(64 * 1024 * 1024)
#define	CREATE_MODE		(0664)
#define	CHECK_BYTES		(4096)

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmemblk_create");

	if (argc != 3)
		FATAL("usage: %s path bsize", argv[0]);

	PMEMblkpool *handle;
	const char *path = argv[1];
	size_t bsize = strtoul(argv[2], NULL, 0);

	int fd;

	if (strcmp(path, "NULLFILE") == 0) {

		if ((handle = pmemblk_create("./testfile", bsize,
				SIZEOF_TESTFILE, CREATE_MODE)) == NULL) {
			OUT("!./testfile: pmemblk_create");
			goto err;
		}

		int result = pmemblk_check("./testfile");

		if (result < 0)
			OUT("!%s: pmemblk_check", "./testfile");
		else if (result == 0)
			OUT("%s: pmemblk_check: not consistent", "./testfile");

		fd = OPEN("./testfile", O_RDWR);

	} else {

		if ((handle = pmemblk_create(path, bsize,
				0, CREATE_MODE)) == NULL) {
			OUT("!%s: pmemblk_create", path);
			goto err;
		}

		int result = pmemblk_check(path);

		if (result < 0)
			OUT("!%s: pmemblk_check", path);
		else if (result == 0)
			OUT("%s: pmemblk_check: not consistent", path);

		fd = OPEN(path, O_RDWR);

	}

	void *addr;

	struct stat stbuf;
	FSTAT(fd, &stbuf);

	char pat[CHECK_BYTES];
	char buf[CHECK_BYTES];

	addr = pmem_map(fd);
	if (addr == NULL) {
		OUT("!pmem_map");
		CLOSE(fd);
		goto err;
	}

	/* write some pattern to the file */
	memset(pat, 0x5A, CHECK_BYTES);
	WRITE(fd, pat, CHECK_BYTES);

	if (memcmp(pat, addr, CHECK_BYTES))
		OUT("first %d bytes of file do not match",
			CHECK_BYTES);

	/* fill up mapped region with new pattern */
	memset(pat, 0xA5, CHECK_BYTES);
	memcpy(addr, pat, CHECK_BYTES);

	MUNMAP(addr, stbuf.st_size);

	LSEEK(fd, (off_t)0, SEEK_SET);
	if (READ(fd, buf, CHECK_BYTES) == CHECK_BYTES) {
		if (memcmp(pat, buf, CHECK_BYTES))
			OUT("first %d bytes of file do not match",
				CHECK_BYTES);
	}

	CLOSE(fd);

err:

	DONE(NULL);
}
