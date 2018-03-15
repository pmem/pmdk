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

	int nallocs = 0;
	struct pobj_action *act = (struct pobj_action *)
		ZALLOC(sizeof(struct pobj_action) * MAX_ACTS);

	do {
		oid = pmemobj_reserve(pop, &act[nallocs++], HUGE_ALLOC_SIZE, 0);
	} while (!OID_IS_NULL(oid));
	pmemobj_cancel(pop, act, nallocs - 1);

	int nallocs2 = 0;
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

	pmemobj_cancel(pop, reserved, 2);

	rootp->published.oid =
		pmemobj_reserve(pop, &published[0], sizeof(struct foo), 0);
	pmemobj_set_value(pop, &published[1], &rootp->published.value, 1);
	pmemobj_publish(pop, published, 2);

	rootp->tx_reserved.oid =
		pmemobj_reserve(pop, &tx_reserved, sizeof(struct foo), 0);

	TX_BEGIN(pop) {
		pmemobj_tx_publish(&tx_reserved, 1);
		pmemobj_tx_abort(EINVAL);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	rootp->tx_reserved_fulfilled.oid =
		pmemobj_reserve(pop,
			&tx_reserved_fulfilled, sizeof(struct foo), 0);

	TX_BEGIN(pop) {
		pmemobj_tx_publish(&tx_reserved_fulfilled, 1);
		pmemobj_tx_publish(NULL, 0); /* this is to force resv fulfill */
		pmemobj_tx_abort(EINVAL);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END

	rootp->tx_published.oid =
		pmemobj_reserve(pop, &tx_published, sizeof(struct foo), 0);

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

	pmemobj_close(pop);

	DONE(NULL);
}
