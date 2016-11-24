/*
 * Copyright 2014-2016, Intel Corporation
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
 * pmem_is_pmem_proc_linux.c -- unit test for pmem_is_pmem() /proc parsing
 *
 * usage: pmem_is_pmem_proc_linux file addr len [addr len]...
 */

#define _GNU_SOURCE
#include "unittest.h"

#include <dlfcn.h>

#define MAX_REGIONS 8
#define MAX_FILES 8
char *Sfile[MAX_FILES];
int Nfiles;
int Nregions;
int Curfile;

struct region {
	uintptr_t addr;
	size_t len;
} Mincore[MAX_REGIONS];

/*
 * fopen -- interpose on libc fopen()
 *
 * This catches opens to /proc/self/smaps and sends them to the fake smaps
 * file being tested.
 */
FILE *
fopen(const char *path, const char *mode)
{
	static FILE *(*fopen_ptr)(const char *path, const char *mode);

	if (strcmp(path, "/proc/self/smaps") == 0) {
		UT_ASSERT(Curfile < Nfiles);
		char *sfile = Sfile[Curfile];
		Curfile = Curfile + 1;

		UT_OUT("redirecting /proc/self/smaps to %s", sfile);
		path = sfile;
	}

	if (fopen_ptr == NULL)
		fopen_ptr = dlsym(RTLD_NEXT, "fopen");

	return (*fopen_ptr)(path, mode);
}

#define IN_RANGE(a, l, A, L)\
	((uintptr_t)(a) >= (uintptr_t)(A) &&\
	(uintptr_t)(a) <= ((uintptr_t)(A) + (L)) &&\
	((uintptr_t)(a) + l) >= (uintptr_t)(A) &&\
	((uintptr_t)(a) + l) <= ((uintptr_t)(A) + (L)))

/*
 * mincore -- interpose on libc mincore(2)
 *
 * Return 0 only for specified regions, otherwise return -1 with
 * errno = ENOMEM.
 */
int
mincore(void *addr, size_t length, unsigned char *vec)
{
	for (int i = 0; i < Nregions; i++) {
		if (IN_RANGE(addr, length, Mincore[i].addr, Mincore[i].len))
			return 0;
	}

	errno = ENOMEM;
	return -1;
}


int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_is_pmem_proc_linux");

	if (argc < 5)
		UT_FATAL("usage: %s nfiles file.. nregions "
			"(addr len)... (addr len)...", argv[0]);

	Nfiles = atoi(argv[1]);
	UT_ASSERT(Nfiles < MAX_FILES);

	for (int i = 0; i < Nfiles; i++) {
		Sfile[i] = argv[2 + i];
	}

	Nregions = atoi(argv[2 + Nfiles]);
	UT_ASSERT(Nregions < MAX_REGIONS);
	for (int i = 0; i < Nregions; i += 2) {
		char *str_addr = argv[3 + Nfiles + i + 0];
		char *str_len  = argv[3 + Nfiles + i + 1];

		Mincore[i].addr = (uintptr_t)strtoull(str_addr, NULL, 16);
		Mincore[i].len = (size_t)strtoull(str_len, NULL, 10);
	}

	for (int arg = 2 + Nfiles + 1 + 2 * Nregions; arg < argc; arg += 2) {
		void *addr;
		size_t len;

		addr = (void *)strtoull(argv[arg], NULL, 16);
		len = (size_t)strtoull(argv[arg + 1], NULL, 10);

		Curfile = 0;

		UT_OUT("addr %p, len %zu: %d", addr, len,
				pmem_is_pmem(addr, len));
	}

	DONE(NULL);
}
