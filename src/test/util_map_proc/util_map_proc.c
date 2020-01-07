// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * util_map_proc.c -- unit test for util_map() /proc parsing
 *
 * usage: util_map_proc maps_file len [len]...
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include "unittest.h"
#include "util.h"
#include "mmap.h"

#define GIGABYTE ((uintptr_t)1 << 30)
#define TERABYTE ((uintptr_t)1 << 40)

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_map_proc");

	util_init();
	util_mmap_init();

	if (argc < 3)
		UT_FATAL("usage: %s maps_file len [len]...", argv[0]);

	Mmap_mapfile = argv[1];
	UT_OUT("redirecting " OS_MAPFILE " to %s", Mmap_mapfile);

	for (int arg = 2; arg < argc; arg++) {
		size_t len = (size_t)strtoull(argv[arg], NULL, 0);

		size_t align = 2 * MEGABYTE;
		if (len >= 2 * GIGABYTE)
			align = GIGABYTE;

		void *h1 =
			util_map_hint_unused((void *)TERABYTE, len, GIGABYTE);
		void *h2 = util_map_hint(len, 0);
		if (h1 != MAP_FAILED && h1 != NULL)
			UT_ASSERTeq((uintptr_t)h1 & (GIGABYTE - 1), 0);
		if (h2 != MAP_FAILED && h2 != NULL)
			UT_ASSERTeq((uintptr_t)h2 & (align - 1), 0);
		if (h1 == NULL) /* XXX portability */
			UT_OUT("len %zu: (nil) %p", len, h2);
		else if (h2 == NULL)
			UT_OUT("len %zu: %p (nil)", len, h1);
		else
			UT_OUT("len %zu: %p %p", len, h1, h2);
	}

	util_mmap_fini();
	DONE(NULL);
}
