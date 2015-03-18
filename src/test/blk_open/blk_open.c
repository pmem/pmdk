/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 * blk_open.c -- unit test for pmemblk_open()
 *
 * usage: blk_open path bsize
 *
 *        if bsize is zero, then create the pool with bsize 4096
 *        but attempt to open it with bsize 2048
 */
#include "unittest.h"

void
pool_check(const char *path)
{
	int result = pmemblk_check(path);

	if (result < 0)
		OUT("!%s: pmemblk_check", path);
	else if (result == 0)
		OUT("%s: pmemblk_check: not consistent", path);
}

void
pool_open(const char *path, size_t bsize)
{
	PMEMblkpool *pbp = pmemblk_open(path, bsize);

	if (pbp == NULL)
		OUT("!%s: pmemblk_open", path);
	else {
		OUT("%s: pmemblk_open: Success", path);

		pmemblk_close(pbp);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_open");

	if (argc != 3)
		FATAL("usage: %s path bsize", argv[0]);

	const char *path = argv[1];
	size_t bsize = strtoul(argv[2], NULL, 0);

	PMEMblkpool *pbp;

	if (bsize)
		pbp = pmemblk_create(path, bsize, (20*1024*1024), 0640);
	else
		pbp = pmemblk_create(path, 4096, (20*1024*1024), 0640);

	if (pbp == NULL)
		OUT("!%s: pmemblk_create", path);
	else {
		struct stat stbuf;
		STAT(path, &stbuf);

		OUT("%s: file size %zu usable blocks %zu mode 0%o",
				path, stbuf.st_size,
				pmemblk_nblock(pbp),
				stbuf.st_mode & 0777);

		pmemblk_close(pbp);

		pool_check(path);

		if (bsize)
			pool_open(path, bsize);
		else
			pool_open(path, 2048);
	}

	if (pbp == NULL) {
		if (bsize) {
			pmemblk_create(path, bsize, 0, 0640);
			pool_check(path);
			pool_open(path, bsize);
		} else {
			pmemblk_create(path, 4096, 0, 0640);
			pool_check(path);
			pool_open(path, 2048);
		}
	}

	DONE(NULL);
}
