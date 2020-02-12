// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * obj_extend.c -- pool extension tests
 *
 */

#include <stddef.h>

#include "unittest.h"

#define ALLOC_SIZE (((1 << 20) * 2) - 16) /* 2 megabytes - 16 bytes (hdr) */
#define RESV_SIZE ((1 << 29) + ((1 << 20) * 8)) /* 512 + 8 megabytes */
#define FRAG 0.9

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_extend");

	if (argc < 2)
		UT_FATAL("usage: %s file-name [alloc-size] [opath]", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, "obj_extend",
			0, S_IWUSR | S_IRUSR)) == NULL) {
		UT_ERR("pmemobj_create: %s", pmemobj_errormsg());
		exit(0);
	}

	size_t alloc_size;
	if (argc > 2)
		alloc_size = ATOUL(argv[2]);
	else
		alloc_size = ALLOC_SIZE;

	const char *opath = path;
	if (argc > 3)
		opath = argv[3];

	size_t allocated = 0;
	PMEMoid oid;
	while (pmemobj_alloc(pop, &oid, alloc_size, 0, NULL, NULL) == 0) {
		allocated += pmemobj_alloc_usable_size(oid);
	}

	UT_ASSERT(allocated > (RESV_SIZE * FRAG));

	pmemobj_close(pop);

	if ((pop = pmemobj_open(opath, "obj_extend")) != NULL) {
		pmemobj_close(pop);

		int result = pmemobj_check(opath, "obj_extend");
		UT_ASSERTeq(result, 1);
	} else {
		UT_ERR("pmemobj_open: %s", pmemobj_errormsg());
	}

	DONE(NULL);
}
