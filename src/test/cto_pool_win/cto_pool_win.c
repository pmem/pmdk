/*
 * Copyright 2017, Intel Corporation
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
 * cto_pool.c -- unit test for pmemcto_create() and pmemcto_open()
 *
 * usage: cto_pool op path layout [poolsize mode]
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
pool_create(const wchar_t *path, const wchar_t *layout, size_t poolsize,
	unsigned mode)
{
	char *upath = ut_toUTF8(path);

	PMEMctopool *pop = pmemcto_createW(path, layout, poolsize, mode);

	if (pop == NULL)
		UT_OUT("!%s: pmemcto_create", upath);
	else {
		os_stat_t stbuf;
		STATW(path, &stbuf);

		UT_OUT("%s: file size %zu mode 0%o",
			upath, stbuf.st_size,
				stbuf.st_mode & 0777);

		pmemcto_close(pop);

		int result = pmemcto_checkW(path, layout);

		if (result < 0)
			UT_OUT("!%s: pmemcto_check", upath);
		else if (result == 0)
			UT_OUT("%s: pmemcto_check: not consistent", upath);
	}
	free(upath);
}

static void
pool_open(const wchar_t *path, const wchar_t *layout)
{
	char *upath = ut_toUTF8(path);
	PMEMctopool *pop = pmemcto_openW(path, layout);
	if (pop == NULL) {
		UT_OUT("!%s: pmemcto_open", upath);
	} else {
		UT_OUT("%s: pmemcto_open: Success", upath);
		pmemcto_close(pop);
	}
	free(upath);
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "cto_pool_win");

	if (argc < 4)
		UT_FATAL("usage: %s op path layout [poolsize mode]",
			ut_toUTF8(argv[0]));

	wchar_t *layout = NULL;
	size_t poolsize;
	unsigned mode;

	if (wcscmp(argv[3], L"EMPTY") == 0)
		layout = L"";
	else if (wcscmp(argv[3], L"NULL") != 0)
		layout = argv[3];

	switch (argv[1][0]) {
	case 'c':
		poolsize = wcstoul(argv[4], NULL, 0) * MB; /* in megabytes */
		mode = wcstoul(argv[5], NULL, 8);

		pool_create(argv[2], layout, poolsize, mode);
		break;

	case 'o':
		pool_open(argv[2], layout);
		break;

	default:
		UT_FATAL("unknown operation");
	}

	DONEW(NULL);
}
