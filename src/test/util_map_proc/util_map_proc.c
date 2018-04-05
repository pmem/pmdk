/*
 * Copyright 2014-2018, Intel Corporation
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
