// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2023, Intel Corporation */

/*
 * obj_ctl_stats.c -- tests for the libpmemobj statistics module
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_stats");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(path, "ctl", PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	int enabled;
	int ret = pmemobj_ctl_get(pop, "stats.enabled", &enabled);
	UT_ASSERTeq(enabled, 0);
	UT_ASSERTeq(ret, 0);

	size_t allocated;
	ret = pmemobj_ctl_get(pop, "stats.heap.curr_allocated", &allocated);
	UT_ASSERTeq(allocated, 0);

	ret = pmemobj_alloc(pop, NULL, 1, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_ctl_get(pop, "stats.heap.curr_allocated", &allocated);
	UT_ASSERTeq(allocated, 0);

	enabled = 1;
	ret = pmemobj_ctl_set(pop, "stats.enabled", &enabled);
	UT_ASSERTeq(ret, 0);

	PMEMoid oid;
	ret = pmemobj_alloc(pop, &oid, 1, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);
	size_t oid_size = pmemobj_alloc_usable_size(oid) + 16;

	ret = pmemobj_ctl_get(pop, "stats.heap.curr_allocated", &allocated);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(allocated, oid_size);

	size_t run_allocated = 0;
	ret = pmemobj_ctl_get(pop, "stats.heap.run_allocated", &run_allocated);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(run_allocated /* 2 allocs */ > allocated /* 1 alloc */);

	pmemobj_free(&oid);

	ret = pmemobj_ctl_get(pop, "stats.heap.curr_allocated", &allocated);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(allocated, 0);

	ret = pmemobj_ctl_get(pop, "stats.heap.run_allocated", &run_allocated);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(run_allocated /* 2 allocs */ > allocated /* 1 alloc */);

	TX_BEGIN(pop) {
		oid = pmemobj_tx_alloc(1, 0);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
	oid_size = pmemobj_alloc_usable_size(oid) + 16;
	ret = pmemobj_ctl_get(pop, "stats.heap.curr_allocated", &allocated);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(allocated, oid_size);

	enum pobj_stats_enabled enum_enabled;
	ret = pmemobj_ctl_get(pop, "stats.enabled", &enum_enabled);
	UT_ASSERTeq(enabled, POBJ_STATS_ENABLED_BOTH);
	UT_ASSERTeq(ret, 0);

	run_allocated = 0;
	ret = pmemobj_ctl_get(pop, "stats.heap.run_allocated", &run_allocated);
	UT_ASSERTeq(ret, 0);

	enum_enabled = POBJ_STATS_ENABLED_PERSISTENT; /* transient disabled */
	ret = pmemobj_ctl_set(pop, "stats.enabled", &enum_enabled);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_alloc(pop, &oid, 1, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	size_t tmp = 0;
	ret = pmemobj_ctl_get(pop, "stats.heap.run_allocated", &tmp);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(tmp, run_allocated); /* shouldn't change */

	/* the deallocated object shouldn't be reflected in rebuilt stats */
	pmemobj_free(&oid);

	pmemobj_close(pop);

	pop = pmemobj_open(path, "ctl");
	UT_ASSERTne(pop, NULL);

	/* stats are rebuilt lazily, so initially this should be 0 */
	tmp = 0;
	ret = pmemobj_ctl_get(pop, "stats.heap.run_allocated", &tmp);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(tmp, 0);

	ret = pmemobj_alloc(pop, NULL, 1, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	/* after first alloc, the previously allocated object will be found */
	tmp = 0;
	ret = pmemobj_ctl_get(pop, "stats.heap.run_allocated", &tmp);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(tmp, run_allocated + oid_size);

	pmemobj_close(pop);

	DONE(NULL);
}
