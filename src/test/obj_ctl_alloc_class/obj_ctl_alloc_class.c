// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * obj_ctl_alloc_class.c -- tests for the ctl entry points: heap.alloc_class
 */

#include <sys/resource.h>
#include "unittest.h"

#define LAYOUT "obj_ctl_alloc_class"

static void
basic(const char *path)
{
	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL * 20,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	int ret;
	PMEMoid oid;
	size_t usable_size;

	struct pobj_alloc_class_desc alloc_class_128;
	alloc_class_128.header_type = POBJ_HEADER_NONE;
	alloc_class_128.unit_size = 128;
	alloc_class_128.units_per_block = 1000;
	alloc_class_128.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.128.desc",
		&alloc_class_128);
	UT_ASSERTeq(ret, 0);

	struct pobj_alloc_class_desc alloc_class_129;
	alloc_class_129.header_type = POBJ_HEADER_COMPACT;
	alloc_class_129.unit_size = 1024;
	alloc_class_129.units_per_block = 1000;
	alloc_class_129.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.129.desc",
		&alloc_class_129);
	UT_ASSERTeq(ret, 0);

	struct pobj_alloc_class_desc alloc_class_128_r;
	ret = pmemobj_ctl_get(pop, "heap.alloc_class.128.desc",
		&alloc_class_128_r);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(alloc_class_128.header_type, alloc_class_128_r.header_type);
	UT_ASSERTeq(alloc_class_128.unit_size, alloc_class_128_r.unit_size);
	UT_ASSERT(alloc_class_128.units_per_block <=
		alloc_class_128_r.units_per_block);

	/*
	 * One unit from alloc class 128 - 128 bytes unit size, minimal headers.
	 */
	ret = pmemobj_xalloc(pop, &oid, 128, 0, POBJ_CLASS_ID(128), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 128);
	pmemobj_free(&oid);

	/*
	 * Reserve as above.
	 */
	struct pobj_action act;
	oid = pmemobj_xreserve(pop, &act, 128, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid));
	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 128);
	pmemobj_cancel(pop, &act, 1);

	/*
	 * One unit from alloc class 128 - 128 bytes unit size, minimal headers,
	 * but request size 1 byte.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1, 0, POBJ_CLASS_ID(128), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 128);
	pmemobj_free(&oid);

	/*
	 * Two units from alloc class 129 -
	 * 1024 bytes unit size, compact headers.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1024 + 1,
		0, POBJ_CLASS_ID(129), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, (1024 * 2) - 16); /* 2 units minus hdr */
	pmemobj_free(&oid);

	/*
	 * 64 units from alloc class 129
	 * - 1024 bytes unit size, compact headers.
	 */
	ret = pmemobj_xalloc(pop, &oid, (1024 * 64) - 16,
		0, POBJ_CLASS_ID(129), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, (1024 * 64) - 16);
	pmemobj_free(&oid);

	/*
	 * 65 units from alloc class 129 -
	 * 1024 bytes unit size, compact headers.
	 * Should fail, as it would require two bitmap modifications.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1024 * 64 + 1, 0,
		POBJ_CLASS_ID(129), NULL, NULL);
	UT_ASSERTeq(ret, -1);

	/*
	 * Nonexistent alloc class.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1, 0, POBJ_CLASS_ID(130), NULL, NULL);
	UT_ASSERTeq(ret, -1);

	struct pobj_alloc_class_desc alloc_class_new;
	alloc_class_new.header_type = POBJ_HEADER_NONE;
	alloc_class_new.unit_size = 777;
	alloc_class_new.units_per_block = 200;
	alloc_class_new.class_id = 0;
	alloc_class_new.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_new);
	UT_ASSERTeq(ret, 0);

	struct pobj_alloc_class_desc alloc_class_fail;
	alloc_class_fail.header_type = POBJ_HEADER_NONE;
	alloc_class_fail.unit_size = 777;
	alloc_class_fail.units_per_block = 200;
	alloc_class_fail.class_id = 0;
	alloc_class_fail.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_fail);
	UT_ASSERTeq(ret, -1);

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.200.desc",
		&alloc_class_fail);
	UT_ASSERTeq(ret, -1);

	ret = pmemobj_xalloc(pop, &oid, 1, 0,
		POBJ_CLASS_ID(alloc_class_new.class_id), NULL, NULL);
	UT_ASSERTeq(ret, 0);
	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 777);

	struct pobj_alloc_class_desc alloc_class_new_huge;
	alloc_class_new_huge.header_type = POBJ_HEADER_NONE;
	alloc_class_new_huge.unit_size = (2 << 23);
	alloc_class_new_huge.units_per_block = 1;
	alloc_class_new_huge.class_id = 0;
	alloc_class_new_huge.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_new_huge);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_xalloc(pop, &oid, 1, 0,
		POBJ_CLASS_ID(alloc_class_new_huge.class_id), NULL, NULL);
	UT_ASSERTeq(ret, 0);
	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, (2 << 23));

	struct pobj_alloc_class_desc alloc_class_new_max;
	alloc_class_new_max.header_type = POBJ_HEADER_COMPACT;
	alloc_class_new_max.unit_size = PMEMOBJ_MAX_ALLOC_SIZE;
	alloc_class_new_max.units_per_block = 1024;
	alloc_class_new_max.class_id = 0;
	alloc_class_new_max.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_new_max);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_xalloc(pop, &oid, 1, 0,
		POBJ_CLASS_ID(alloc_class_new_max.class_id), NULL, NULL);
	UT_ASSERTne(ret, 0);

	struct pobj_alloc_class_desc alloc_class_new_loop;
	alloc_class_new_loop.header_type = POBJ_HEADER_COMPACT;
	alloc_class_new_loop.unit_size = 16384;
	alloc_class_new_loop.units_per_block = 63;
	alloc_class_new_loop.class_id = 0;
	alloc_class_new_loop.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_new_loop);
	UT_ASSERTeq(ret, 0);

	size_t s = (63 * 16384) - 16;
	ret = pmemobj_xalloc(pop, &oid, s + 1, 0,
		POBJ_CLASS_ID(alloc_class_new_loop.class_id), NULL, NULL);
	UT_ASSERTne(ret, 0);

	struct pobj_alloc_class_desc alloc_class_tiny;
	alloc_class_tiny.header_type = POBJ_HEADER_NONE;
	alloc_class_tiny.unit_size = 7;
	alloc_class_tiny.units_per_block = 1;
	alloc_class_tiny.class_id = 0;
	alloc_class_tiny.alignment = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_tiny);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(alloc_class_tiny.units_per_block > 1);

	for (int i = 0; i < 1000; ++i) {
		ret = pmemobj_xalloc(pop, &oid, 7, 0,
			POBJ_CLASS_ID(alloc_class_tiny.class_id), NULL, NULL);
		UT_ASSERTeq(ret, 0);
	}

	pmemobj_close(pop);
}

static void
many(const char *path)
{
	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	unsigned nunits = UINT16_MAX + 1;

	struct pobj_alloc_class_desc alloc_class_tiny;
	alloc_class_tiny.header_type = POBJ_HEADER_NONE;
	alloc_class_tiny.unit_size = 8;
	alloc_class_tiny.units_per_block = nunits;
	alloc_class_tiny.class_id = 0;
	alloc_class_tiny.alignment = 0;
	int ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_tiny);
	UT_ASSERTeq(ret, 0);

	PMEMoid oid;
	uint64_t *counterp = NULL;
	for (size_t i = 0; i < nunits; ++i) {
		pmemobj_xalloc(pop, &oid, 8, 0,
			POBJ_CLASS_ID(alloc_class_tiny.class_id), NULL, NULL);
		counterp = pmemobj_direct(oid);
		(*counterp)++;
		/*
		 * This works only because this is a fresh pool in a new file
		 * and so the counter must be initially zero.
		 * This might have to be fixed if that ever changes.
		 */
		UT_ASSERTeq(*counterp, 1);
	}

	pmemobj_close(pop);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_alloc_class");

	if (argc != 3)
		UT_FATAL("usage: %s file-name b|m", argv[0]);

	const char *path = argv[1];
	if (argv[2][0] == 'b')
		basic(path);
	else if (argv[2][0] == 'm')
		many(path);

	DONE(NULL);
}
