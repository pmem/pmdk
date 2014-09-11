/*
 * Copyright (c) 2014, Intel Corporation
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
 * util_map_proc.c -- unit test for util_map() /proc parsing
 *
 * usage: util_map_proc maps_file len [len]...
 */

#define	_GNU_SOURCE

#include <dlfcn.h>
#include "unittest.h"
#include "util_wrap.h"


char *Sfile;

/*
 * fopen -- interpose on libc fopen()
 *
 * This catches opens to /proc/self/maps and sends them to the fake maps
 * file being tested.
 */
FILE *
fopen(const char *path, const char *mode)
{
	static FILE *(*fopen_ptr)(const char *path, const char *mode);

	if (strcmp(path, "/proc/self/maps") == 0) {
		OUT("redirecting /proc/self/maps to %s", Sfile);
		path = Sfile;
	}

	if (fopen_ptr == NULL)
		fopen_ptr = dlsym(RTLD_NEXT, "fopen");

	return (*fopen_ptr)(path, mode);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_map_proc");

	if (argc < 3)
		FATAL("usage: %s maps_file len [len]...", argv[0]);

	Sfile = argv[1];

	for (int arg = 2; arg < argc; arg++) {
		size_t len;

		len = (size_t)strtoull(argv[arg], NULL, 0);
		OUT("len %zu: %p", len, util_map_hint_wrap(len));
	}

	DONE(NULL);
}
