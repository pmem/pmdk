// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

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
#define MAX_OVERALL_OVERHEAD 0.10f

/*
 * For the best accuracy fragmentation should be measured for one full zone
 * because the metadata is preallocated. For reasonable test duration a smaller
 * size must be used.
 */
#define DEFAULT_FILE_SIZE ((size_t)(1ULL << 28)) /* 256 megabytes */

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_fragmentation");

	if (argc < 3)
		UT_FATAL("usage: %s allocsize filename [filesize]", argv[0]);

	size_t file_size;
	if (argc == 4)
		file_size = ATOUL(argv[3]);
	else
		file_size = DEFAULT_FILE_SIZE;

	size_t alloc_size = ATOUL(argv[1]);
	const char *path = argv[2];

	PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME, file_size,
				S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	size_t allocated = 0;
	int err = 0;
	do {
		PMEMoid oid;
		err = pmemobj_alloc(pop, &oid, alloc_size, 0, NULL, NULL);
		if (err == 0)
			allocated += pmemobj_alloc_usable_size(oid) +
				OBJECT_OVERHEAD;
	} while (err == 0);

	float allocated_pct = ((float)allocated / file_size);
	float overhead_pct = 1.f - allocated_pct;
	UT_ASSERT(overhead_pct <= MAX_OVERALL_OVERHEAD);

	pmemobj_close(pop);

	DONE(NULL);
}
