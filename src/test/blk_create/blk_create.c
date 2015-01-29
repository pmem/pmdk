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
 * blk_create.c -- unit test for pmemblk_create()
 *
 * usage: blk_create path bsize poolsize mode
 *
 */
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_create");

	if (argc != 5)
		FATAL("usage: %s path bsize poolsize mode", argv[0]);

	const char *path = argv[1];
	size_t bsize = strtoul(argv[2], NULL, 0);
	size_t poolsize = strtoul(argv[3], NULL, 0)
		* (1 << 20); /* in megabytes */
	size_t mode = strtoul(argv[4], NULL, 8);

	PMEMblkpool *pbp = pmemblk_create(path, bsize, poolsize, mode);

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

		int result = pmemblk_check(path);

		if (result < 0)
			OUT("!%s: pmemblk_check", path);
		else if (result == 0)
			OUT("%s: pmemblk_check: not consistent", path);
	}

	DONE(NULL);
}
