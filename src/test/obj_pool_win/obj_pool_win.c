// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * obj_pool.c -- unit test for pmemobj_create() and pmemobj_open()
 *
 * usage: obj_pool op path layout [poolsize mode]
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

	PMEMobjpool *pop = pmemobj_createW(path, layout, poolsize, mode);

	if (pop == NULL)
		UT_OUT("!%s: pmemobj_create", upath);
	else {
		os_stat_t stbuf;
		STATW(path, &stbuf);

		UT_OUT("%s: file size %zu mode 0%o",
			upath, stbuf.st_size,
				stbuf.st_mode & 0777);

		pmemobj_close(pop);

		int result = pmemobj_checkW(path, layout);

		if (result < 0)
			UT_OUT("!%s: pmemobj_check", upath);
		else if (result == 0)
			UT_OUT("%s: pmemobj_check: not consistent", upath);
	}
	free(upath);
}

static void
pool_open(const wchar_t *path, const wchar_t *layout)
{
	char *upath = ut_toUTF8(path);
	PMEMobjpool *pop = pmemobj_openW(path, layout);
	if (pop == NULL) {
		UT_OUT("!%s: pmemobj_open", upath);
	} else {
		UT_OUT("%s: pmemobj_open: Success", upath);
		pmemobj_close(pop);
	}
	free(upath);
}

int
wmain(int argc, wchar_t *argv[])
{
	STARTW(argc, argv, "obj_pool_win");

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
