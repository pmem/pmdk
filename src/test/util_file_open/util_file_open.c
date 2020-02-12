// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * util_file_open.c -- unit test for util_file_open()
 *
 * usage: util_file_open minlen path [path]...
 */

#include "unittest.h"

#include "file.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_file_open");

	if (argc < 3)
		UT_FATAL("usage: %s minlen path...", argv[0]);

	char *fname;
	size_t minsize = strtoul(argv[1], &fname, 0);

	for (int arg = 2; arg < argc; arg++) {
		size_t size = 0;
		int fd = util_file_open(argv[arg], &size, minsize, O_RDWR);
		if (fd == -1)
			UT_OUT("!%s: util_file_open", argv[arg]);
		else {
			UT_OUT("%s: open, len %zu", argv[arg], size);
			os_close(fd);
		}
	}

	DONE(NULL);
}
