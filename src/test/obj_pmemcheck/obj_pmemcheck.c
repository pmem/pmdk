// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

#include "unittest.h"
#include "valgrind_internal.h"

struct foo {
	PMEMmutex bar;
};

static void
test_mutex_pmem_mapping_register(PMEMobjpool *pop)
{
	PMEMoid foo;
	int ret = pmemobj_alloc(pop, &foo, sizeof(struct foo), 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(foo));
	struct foo *foop = pmemobj_direct(foo);
	ret = pmemobj_mutex_lock(pop, &foop->bar);
	/* foo->bar has been removed from pmem mappings collection */
	VALGRIND_PRINT_PMEM_MAPPINGS;

	UT_ASSERTeq(ret, 0);
	ret = pmemobj_mutex_unlock(pop, &foop->bar);
	UT_ASSERTeq(ret, 0);
	pmemobj_free(&foo);
	/* the entire foo object has been re-registered as pmem mapping */
	VALGRIND_PRINT_PMEM_MAPPINGS;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmemcheck");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], "pmemcheck", PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	test_mutex_pmem_mapping_register(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
