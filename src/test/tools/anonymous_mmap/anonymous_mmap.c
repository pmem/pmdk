// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2024, Intel Corporation */

/*
 * anonymous_mmap.c -- tool for verifying if given memory length can be
 *			anonymously mmapped
 */

#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include "out.h"

int
main(int argc, char *argv[])
{
	out_init("ANONYMOUS_MMAP", "ANONYMOUS_MMAP", "", 1, 0);

	if (argc != 2) {
		printf("Usage: %s <length>\n", argv[0]);
		return -1;
	}

	const size_t length = (size_t)atoll(argv[1]);
	char *addr = mmap(NULL, length, PROT_READ,
				MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		printf(
			"anonymous_mmap.c: Failed to mmap length=%lu of memory, errno=%d\n",
			length, errno);
		return errno;
	}

	out_fini();

	return 0;
}
