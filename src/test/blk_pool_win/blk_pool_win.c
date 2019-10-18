/*
 * Copyright 2015-2019, Intel Corporation
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
 * blk_pool_win.c -- unit test for pmemblk_create() and pmemblk_open()
 *
 * usage: blk_pool_win op path bsize [poolsize mode]
 *
 * op can be:
 *   c - create
 *   o - open
 *
 * "poolsize" and "mode" arguments are ignored for "open"
 */
#include "unittest.h"

#define MB ((size_t)1 << 20)

static void
pool_create(const wchar_t *path, size_t bsize, size_t poolsize, unsigned mode)
{
	char *upath = ut_toUTF8(path);
	UT_ASSERTne(upath, NULL);

	PMEMblkpool *pbp = pmemblk_createW(path, bsize, poolsize, mode);
	if (pbp == NULL)
		UT_OUT("!%s: pmemblk_create", upath);
	else {
		os_stat_t stbuf;
		STATW(path, &stbuf);

		UT_OUT("%s: file size %zu usable blocks %zu mode 0%o",
				upath, stbuf.st_size,
				pmemblk_nblock(pbp),
				stbuf.st_mode & 0777);

		pmemblk_close(pbp);

		int result = pmemblk_checkW(path, bsize);

		if (result < 0)
			UT_OUT("!%s: pmemblk_check", upath);
		else if (result == 0)
			UT_OUT("%s: pmemblk_check: not consistent", upath);
		else
			UT_ASSERTeq(pmemblk_checkW(path, bsize * 2), -1);

		free(upath);
	}
}

static void
pool_open(const wchar_t *path, size_t bsize)
{
	char *upath = ut_toUTF8(path);
	UT_ASSERTne(upath, NULL);

	PMEMblkpool *pbp = pmemblk_openW(path, bsize);
	if (pbp == NULL)
		UT_OUT("!%s: pmemblk_open", upath);
	else {
		UT_OUT("%s: pmemblk_open: Success", upath);
		pmemblk_close(pbp);
	}
	free(upath);
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "blk_pool_win");

	if (argc < 4)
		UT_FATAL("usage: %s op path bsize [poolsize mode]",
			ut_toUTF8(argv[0]));

	size_t bsize = wcstoul(argv[3], NULL, 0);
	size_t poolsize;
	unsigned mode;

	switch (argv[1][0]) {
	case 'c':
		poolsize = wcstoul(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = wcstoul(argv[5], NULL, 8);

		pool_create(argv[2], bsize, poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2], bsize);
		break;

	default:
		UT_FATAL("unknown operation");
	}

	DONEW(NULL);
}
