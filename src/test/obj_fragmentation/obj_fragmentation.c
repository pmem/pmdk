/*
 * Copyright 2015-2016, Intel Corporation
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
 * obj_fragmentation.c -- measures average heap fragmentation
 *
 * A pretty simplistic test that measures internal fragmentation of the
 * allocator for the given size.
 */

#include <stdlib.h>
#include "unittest.h"

#define LAYOUT_NAME "obj_fragmentation"
#define OBJECT_OVERHEAD 64 /* account for the header added to each object */
#define MAX_OVERALL_OVERHEAD 0.20f

/*
 * For the best accuracy fragmentation should be measured for one full zone
 * because the metadata is preallocated. For reasonable test duration a smaller
 * size must be used.
 */
#define DEFAULT_FILE_SIZE (1 << 28) /* 256 megabytes */

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_fragmentation");

	if (argc < 3)
		UT_FATAL("usage: %s allocsize filename [filesize]", argv[0]);

	size_t file_size;
	if (argc == 4)
		file_size = atoll(argv[3]);
	else
		file_size = DEFAULT_FILE_SIZE;

	size_t alloc_size = atoll(argv[1]);
	const char *path = argv[2];

	PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME, file_size,
				S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	size_t n;
	int err = 0;
	for (n = 0; err == 0; n += alloc_size + OBJECT_OVERHEAD) {
		err = pmemobj_alloc(pop, NULL, alloc_size, 0, NULL, NULL);
	}

	float allocated_pct = ((float)n / file_size);
	float overhead_pct = 1.f - allocated_pct;
	UT_ASSERT(overhead_pct - MAX_OVERALL_OVERHEAD <= 0.f);

	pmemobj_close(pop);

	DONE(NULL);
}
