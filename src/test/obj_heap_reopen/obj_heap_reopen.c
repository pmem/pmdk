// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * obj_heap_reopen.c -- test for reopening an existing heap and deallocating
 * objects prior to any allocations to validate the memory reclamation process.
 */

#include <stddef.h>

#include "libpmemobj/action_base.h"
#include "libpmemobj/atomic_base.h"
#include "out.h"
#include "unittest.h"
#include "obj.h"

#define TEST_OBJECT_SIZE (4 << 20)

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_heap_reopen");

	if (argc < 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(basic),
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid oid;
	pmemobj_alloc(pop, &oid, 4 << 20, 0, NULL, NULL);

	pmemobj_close(pop);

	if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(basic))) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	uint64_t freed_oid_off = oid.off;
	pmemobj_free(&oid);

	struct pobj_action act;
	oid = pmemobj_reserve(pop, &act, TEST_OBJECT_SIZE, 0);
	UT_ASSERTeq(oid.off, freed_oid_off);

	for (;;) {
		PMEMoid oid2;
		if (pmemobj_alloc(pop, &oid2, 1, 0, NULL, NULL) != 0)
			break;
		UT_ASSERT(!(oid2.off >= oid.off &&
			oid2.off <= oid.off + TEST_OBJECT_SIZE));
	}

	pmemobj_publish(pop, &act, 1);

	pmemobj_close(pop);

	DONE(NULL);
}
