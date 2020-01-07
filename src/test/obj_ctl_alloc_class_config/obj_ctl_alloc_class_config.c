// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * obj_ctl_alloc_class_config.c -- tests for the ctl alloc class config
 */

#include "unittest.h"

#define LAYOUT "obj_ctl_alloc_class_config"

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_alloc_class_config");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
			S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	struct pobj_alloc_class_desc alloc_class;
	int ret;

	ret = pmemobj_ctl_get(pop, "heap.alloc_class.128.desc", &alloc_class);
	UT_ASSERTeq(ret, 0);

	UT_OUT("%d %lu %d", alloc_class.header_type, alloc_class.unit_size,
		alloc_class.units_per_block);

	ret = pmemobj_ctl_get(pop, "heap.alloc_class.129.desc", &alloc_class);
	UT_ASSERTeq(ret, 0);

	UT_OUT("%d %lu %d", alloc_class.header_type, alloc_class.unit_size,
		alloc_class.units_per_block);

	ret = pmemobj_ctl_get(pop, "heap.alloc_class.130.desc", &alloc_class);
	UT_ASSERTeq(ret, 0);

	UT_OUT("%d %lu %d", alloc_class.header_type, alloc_class.unit_size,
		alloc_class.units_per_block);

	pmemobj_close(pop);

	DONE(NULL);
}
