// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * obj_ctl_alignment.c -- tests for the alloc class alignment
 */

#include "unittest.h"

#define LAYOUT "obj_ctl_alignment"

static PMEMobjpool *pop;

static void
test_fail(void)
{
	struct pobj_alloc_class_desc ac;
	ac.header_type = POBJ_HEADER_NONE;
	ac.unit_size = 1024 - 1;
	ac.units_per_block = 100;
	ac.alignment = 512;

	int ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc", &ac);
	UT_ASSERTeq(ret, -1); /* unit_size must be multiple of alignment */
}

static void
test_aligned_allocs(size_t size, size_t alignment, enum pobj_header_type htype)
{
	struct pobj_alloc_class_desc ac;
	ac.header_type = htype;
	ac.unit_size = size;
	ac.units_per_block = 100;
	ac.alignment = alignment;

	int ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc", &ac);
	UT_ASSERTeq(ret, 0);

	PMEMoid oid;
	ret = pmemobj_xalloc(pop, &oid, 1, 0,
		POBJ_CLASS_ID(ac.class_id), NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(oid.off % alignment, 0);
	UT_ASSERTeq((uintptr_t)pmemobj_direct(oid) % alignment, 0);

	ret = pmemobj_xalloc(pop, &oid, 1, 0,
		POBJ_CLASS_ID(ac.class_id), NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(oid.off % alignment, 0);
	UT_ASSERTeq((uintptr_t)pmemobj_direct(oid) % alignment, 0);

	char query[1024];
	snprintf(query, 1024, "heap.alloc_class.%u.desc", ac.class_id);

	struct pobj_alloc_class_desc read_ac;
	ret = pmemobj_ctl_get(pop, query, &read_ac);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ac.alignment, read_ac.alignment);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_alignment");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL * 10,
			S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemobj_create: %s", path);

	test_fail();
	test_aligned_allocs(1024, 512, POBJ_HEADER_NONE);
	test_aligned_allocs(1024, 512, POBJ_HEADER_COMPACT);
	test_aligned_allocs(64, 64, POBJ_HEADER_COMPACT);

	pmemobj_close(pop);

	DONE(NULL);
}
