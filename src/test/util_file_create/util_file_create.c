// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * util_file_create.c -- unit test for util_file_create()
 *
 * usage: util_file_create minlen len:path [len:path]...
 */

#include "unittest.h"
#include "file.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_file_create");

	if (argc < 3)
		UT_FATAL("usage: %s minlen len:path...", argv[0]);

	char *fname;
	size_t minsize = strtoul(argv[1], &fname, 0);

	for (int arg = 2; arg < argc; arg++) {
		size_t size = strtoul(argv[arg], &fname, 0);
		if (*fname != ':')
			UT_FATAL("usage: %s minlen len:path...", argv[0]);
		fname++;

		int fd;
		if ((fd = util_file_create(fname, size, minsize)) == -1)
			UT_OUT("!%s: util_file_create", fname);
		else {
			UT_OUT("%s: created", fname);
			os_close(fd);
		}
	}

	DONE(NULL);
}
