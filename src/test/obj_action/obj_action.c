/*
 * Copyright 2017-2018, Intel Corporation
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
 * obj_action.c -- test the action API
 */

#include <stdlib.h>
#include "unittest.h"

#define LAYOUT_NAME "obj_action"

struct macro_reserve_s {
	PMEMoid oid;
	uint64_t value;
};

TOID_DECLARE(struct macro_reserve_s, 1);

struct foo {
	int bar;
};

struct root {
	struct {
		PMEMoid oid;
		uint64_t value;
	} reserved;

	struct {
		PMEMoid oid;
		uint64_t value;
	} published;

	struct {
		PMEMoid oid;
	} tx_reserved;

	struct {
		PMEMoid oid;
	} tx_reserved_fulfilled;

	struct {
		PMEMoid oid;
	} tx_published;
};

#define HUGE_ALLOC_SIZE ((1 << 20) * 3)
#define MAX_ACTS 10

static void
test_resv_cancel_huge(PMEMobjpool *pop)
{
	PMEMoid oid;

	unsigned nallocs = 0;
	struct pobj_action *act = (struct pobj_action *)
		ZALLOC(sizeof(struct pobj_action) * MAX_ACTS);

	do {
		oid = pmemobj_reserve(pop, &act[nallocs++], HUGE_ALLOC_SIZE, 0);
	} while (!OID_IS_NULL(oid));
	pmemobj_cancel(pop, act, nallocs - 1);

	unsigned nallocs2 = 0;
	do {
		oid = pmemobj_reserve(pop, &act[nallocs2++],
			HUGE_ALLOC_SIZE, 0);
	} while (!OID_IS_NULL(oid));
	pmemobj_cancel(pop, act, nallocs2 - 1);

	UT_ASSERTeq(nallocs, nallocs2);

	FREE(act);
}

static void
test_defer_free(PMEMobjpool *pop)
{
	PMEMoid oid;

	int ret = pmemobj_alloc(pop, &oid, sizeof(struct foo), 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	struct pobj_action act;
	pmemobj_defer_free(pop, oid, &act);

	pmemobj_publish(pop, &act, 1);

	struct foo *f = (struct foo *)pmemobj_direct(oid);
	f->bar = 5; /* should trigger memcheck error */

	ret = pmemobj_alloc(pop, &oid, sizeof(struct foo), 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);

	pmemobj_defer_free(pop, oid, &act);

	pmemobj_cancel(pop, &act, 1);
	f = (struct foo *)pmemobj_direct(oid);
	f->bar = 5; /* should NOT trigger memcheck error */
}

/*
 * This function tests if macros included in action.h api compile and
 * allocate memory.
 */
static void
test_api_macros(PMEMobjpool *pop)
{
	struct pobj_action macro_reserve_act[1];

	TOID(struct macro_reserve_s) macro_reserve_p = POBJ_RESERVE_NEW(pop,
		struct macro_reserve_s, &macro_reserve_act[0]);
	UT_ASSERT(!OID_IS_NULL(macro_reserve_p.oid));
	pmemobj_publish(pop, macro_reserve_act, 1);
	POBJ_FREE(&macro_reserve_p);

	macro_reserve_p = POBJ_RESERVE_ALLOC(pop, struct macro_reserve_s,
		sizeof(struct macro_reserve_s), &macro_reserve_act[0]);
	UT_ASSERT(!OID_IS_NULL(macro_reserve_p.oid));
	pmemobj_publish(pop, macro_reserve_act, 1);
	POBJ_FREE(&macro_reserve_p);

	macro_reserve_p = POBJ_XRESERVE_NEW(pop, struct macro_reserve_s,
		&macro_reserve_act[0], 0);
	UT_ASSERT(!OID_IS_NULL(macro_reserve_p.oid));
	pmemobj_publish(pop, macro_reserve_act, 1);
	POBJ_FREE(&macro_reserve_p);

	macro_reserve_p = POBJ_XRESERVE_ALLOC(pop, struct macro_reserve_s,
		sizeof(struct macro_reserve_s), &macro_reserve_act[0], 0);
	UT_ASSERT(!OID_IS_NULL(macro_reserve_p.oid));
	pmemobj_publish(pop, macro_reserve_act, 1);
	POBJ_FREE(&macro_reserve_p);
}

#define POBJ_MAX_ACTIONS 60

static void
test_many(PMEMobjpool *pop, size_t n)
{
	struct pobj_action *act = (struct pobj_action *)
		MALLOC(sizeof(struct pobj_action) * n);
	PMEMoid *oid = (PMEMoid *)
		MALLOC(sizeof(PMEMoid) * n);

	for (int i = 0; i < n; ++i) {
		oid[i] = pmemobj_reserve(pop, &act[i], 1, 0);
		UT_ASSERT(!OID_IS_NULL(oid[i]));
	}

	UT_ASSERTeq(pmemobj_publish(pop, act, n), 0);

	for (int i = 0; i < n; ++i) {
		pmemobj_defer_free(pop, oid[i], &act[i]);
	}

	UT_ASSERTeq(pmemobj_publish(pop, act, n), 0);

	FREE(oid);
	FREE(act);
}

static void
test_duplicate(PMEMobjpool *pop)
{
	struct pobj_alloc_class_desc alloc_class_128;
	alloc_class_128.header_type = POBJ_HEADER_COMPACT;
	alloc_class_128.unit_size = 1024 * 100;
	alloc_class_128.units_per_block = 1;
	alloc_class_128.alignment = 0;

	int ret = pmemobj_ctl_set(pop, "heap.alloc_class.128.desc",
		&alloc_class_128);
	UT_ASSERTeq(ret, 0);

	struct pobj_action a[10];
	PMEMoid oid[10];

	oid[0] = pmemobj_xreserve(pop, &a[0], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	pmemobj_cancel(pop, a, 1);

	oid[0] = pmemobj_xreserve(pop, &a[0], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	oid[0] = pmemobj_xreserve(pop, &a[1], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	oid[0] = pmemobj_xreserve(pop, &a[2], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	pmemobj_cancel(pop, a, 3);

	oid[0] = pmemobj_xreserve(pop, &a[0], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	oid[0] = pmemobj_xreserve(pop, &a[1], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	oid[0] = pmemobj_xreserve(pop, &a[2], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	oid[0] = pmemobj_xreserve(pop, &a[3], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	oid[0] = pmemobj_xreserve(pop, &a[4], 1, 0, POBJ_CLASS_ID(128));
	UT_ASSERT(!OID_IS_NULL(oid[0]));

	pmemobj_cancel(pop, a, 5);
}

static void
test_many_sets(PMEMobjpool *pop, size_t n)
{
	struct pobj_action *act = (struct pobj_action *)
		MALLOC(sizeof(struct pobj_action) * n);
	PMEMoid oid;
	pmemobj_alloc(pop, &oid, sizeof(uint64_t) * n, 0, NULL, NULL);
	UT_ASSERT(!OID_IS_NULL(oid));

	uint64_t *values = (uint64_t *)pmemobj_direct(oid);

	for (uint64_t i = 0; i < n; ++i)
		pmemobj_set_value(pop, &act[i], values + i, i);

	UT_ASSERTeq(pmemobj_publish(pop, act, n), 0);

	for (uint64_t i = 0; i < n; ++i)
		UT_ASSERTeq(*(values + i), i);

	pmemobj_free(&oid);
	FREE(act);
}


int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_action");

	if (argc < 2)
		UT_FATAL("usage: %s filename", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME, PMEMOBJ_MIN_POOL,
				S_IWUSR | S_IRUSR);
	if (pop == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid root = pmemobj_root(pop, sizeof(struct root));
	struct root *rootp = (struct root *)pmemobj_direct(root);

	struct pobj_action reserved[2];
	struct pobj_action published[2];
	struct pobj_action tx_reserved;
	struct pobj_action tx_reserved_fulfilled;
	struct pobj_action tx_published;

	rootp->reserved.oid =
		pmemobj_reserve(pop, &reserved[0], sizeof(struct foo), 0);
	pmemobj_set_value(pop, &reserved[1], &rootp->reserved.value, 1);

	rootp->tx_reserved.oid =
		pmemobj_reserve(pop, &tx_reserved, sizeof(struct foo), 0);

	rootp->tx_reserved_fulfilled.oid =
		pmemobj_reserve(pop,
			&tx_reserved_fulfilled, sizeof(struct foo), 0);

	rootp->tx_published.oid =
		pmemobj_reserve(pop, &tx_published, sizeof(struct foo), 0);

	rootp->published.oid =
		pmemobj_reserve(pop, &published[0], sizeof(struct foo), 0);

	TX_BEGIN(pop) {
		pmemobj_tx_publish(&tx_reserved, 1);
		pmemobj_tx_abort(EINVAL);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		pmemobj_tx_publish(&tx_reserved_fulfilled, 1);
		pmemobj_tx_publish(NULL, 0); /* this is to force resv fulfill */
		pmemobj_tx_abort(EINVAL);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	pmemobj_set_value(pop, &published[1], &rootp->published.value, 1);
	pmemobj_publish(pop, published, 2);

	TX_BEGIN(pop) {
		pmemobj_tx_publish(&tx_published, 1);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	pmemobj_persist(pop, rootp, sizeof(*rootp));

	pmemobj_close(pop);

	UT_ASSERTeq(pmemobj_check(path, LAYOUT_NAME), 1);

	UT_ASSERTne(pop = pmemobj_open(path, LAYOUT_NAME), NULL);

	root = pmemobj_root(pop, sizeof(struct root));
	rootp = (struct root *)pmemobj_direct(root);

	struct foo *reserved_foop =
		(struct foo *)pmemobj_direct(rootp->reserved.oid);
	reserved_foop->bar = 1; /* should trigger memcheck error */

	UT_ASSERTeq(rootp->reserved.value, 0);

	struct foo *published_foop =
		(struct foo *)pmemobj_direct(rootp->published.oid);
	published_foop->bar = 1; /* should NOT trigger memcheck error */

	UT_ASSERTeq(rootp->published.value, 1);

	struct foo *tx_reserved_foop =
		(struct foo *)pmemobj_direct(rootp->tx_reserved.oid);
	tx_reserved_foop->bar = 1; /* should trigger memcheck error */

	struct foo *tx_reserved_fulfilled_foop =
		(struct foo *)pmemobj_direct(rootp->tx_reserved_fulfilled.oid);
	tx_reserved_fulfilled_foop->bar = 1; /* should trigger memcheck error */

	struct foo *tx_published_foop =
		(struct foo *)pmemobj_direct(rootp->tx_published.oid);
	tx_published_foop->bar = 1; /* should NOT trigger memcheck error */

	test_resv_cancel_huge(pop);

	test_defer_free(pop);

	test_api_macros(pop);

	test_many(pop, POBJ_MAX_ACTIONS * 2);
	test_many_sets(pop, POBJ_MAX_ACTIONS * 2);

	test_duplicate(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
