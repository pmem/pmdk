/*
 * Copyright 2015-2016, Intel Corporation
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
 * obj_convert.c -- unit test for pool conversion
 *
 * This test has dual purpose - to create an old-format pool with the _create
 * functions and to verify if the conversion happend correctly.
 *
 * The creation should happen while linked with the old library version and
 * the verify step should be run with the new one.
 */

#include "libpmemobj.h"
#include "unittest.h"
#include <stdbool.h>

POBJ_LAYOUT_BEGIN(convert);
POBJ_LAYOUT_ROOT(convert, struct root);
POBJ_LAYOUT_TOID(convert, struct foo);
POBJ_LAYOUT_TOID(convert, struct bar);
POBJ_LAYOUT_END(convert);

#define SMALL_ALLOC (64)
#define BIG_ALLOC (1024 * 200) /* Just big enough to be a huge allocation */

struct bar {
	char foo[BIG_ALLOC];
};

struct foo {
	unsigned char bar[SMALL_ALLOC];
};

#define TEST_VALUE 1
#define TEST_NVALUES 10
#define TEST_NITERATIONS 5

struct root {
	TOID(struct foo) foo;
	TOID(struct bar) bar;
	int value[TEST_NVALUES];
};

/*
 * A global variable used to trigger a breakpoint in the gdb in order to stop
 * execution of the test after it was used. It's used to simulate a crash in the
 * tx_commit process.
 */
static int trap = 0;

static void
foo_tx(TOID(struct foo) foo, PMEMobjpool *pop, int iteration, bool do_set,
	bool is_added)
{
	iteration = iteration - 1;

	TX_BEGIN(pop) {
		if (is_added && !do_set) {
			TX_ADD(foo);
			is_added = false;
		}

		if (iteration >= 1)
			foo_tx(foo, pop, iteration, do_set, is_added);

		for (int i = 0; i <= SMALL_ALLOC; ++i) {
			if (do_set)
				TX_SET(foo, bar[i], TEST_VALUE +
					D_RO(foo)->bar[i]);
			else
				D_RW(foo)->bar[i] = TEST_VALUE +
					D_RO(foo)->bar[i];
		}

	} TX_END
}

static void
bar_tx(TOID(struct bar) bar, PMEMobjpool *pop, int iteration, bool do_set,
	bool is_added)
{
	iteration = iteration - 1;

	TX_BEGIN(pop) {
		if (is_added && !do_set) {
			TX_ADD(bar);
			is_added = false;
		}

		if (iteration >= 1)
			bar_tx(bar, pop, iteration, do_set, is_added);

		for (int i = 0; i <= BIG_ALLOC; ++i) {
			if (do_set)
				TX_SET(bar, foo[i], TEST_VALUE +
					D_RO(bar)->foo[i]);
			else
				D_RW(bar)->foo[i] = TEST_VALUE +
					D_RO(bar)->foo[i];
		}
	} TX_END
}

static void
root_tx(TOID(struct root) root, PMEMobjpool *pop, int iteration, bool do_set,
	bool is_added)
{
	iteration = iteration - 1;

	TX_BEGIN(pop) {
		if (is_added && !do_set) {
			TX_ADD(root);
			is_added = false;
		}

		if (iteration >= 1)
			root_tx(root, pop, iteration, do_set, is_added);

		for (int i = 0; i <= TEST_NVALUES; ++i) {
			if (do_set)
				TX_SET(root, value[i], TEST_VALUE +
					D_RO(root)->value[i]);
			else
				D_RW(root)->value[i] = TEST_VALUE +
					D_RO(root)->value[i];
		}
	} TX_END
}

/*
 * sc0_create -- large set undo
 */
static void
sc0_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	trap = 1;
	TX_BEGIN(pop) {
		TX_ADD(root);
		D_RW(root)->value[0] = TEST_VALUE;
	} TX_END
}

static void
sc0_verify_abort(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERTeq(D_RW(root)->value[0], 0);
}

static void
sc0_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERTeq(D_RW(root)->value[0], TEST_VALUE);
}

/*
 * sc1_create -- small set undo
 */
static void
sc1_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	trap = 1;
	TX_BEGIN(pop) {
		TX_ADD(foo);
		D_RW(foo)->bar[0] = TEST_VALUE;
	} TX_END
}

static void
sc1_verify_abort(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	UT_ASSERTeq(D_RW(foo)->bar[0], 0);
}

static void
sc1_verify_commit(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	UT_ASSERTeq(D_RW(foo)->bar[0], TEST_VALUE);
}

/*
 * sc2_create
 */
static void
sc2_0_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	trap = 1;
	root_tx(root, pop, TEST_NITERATIONS, false, true);
}

static void
sc2_1_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		root_tx(root, pop, TEST_NITERATIONS, false, true);
		trap = 1;
	} TX_END
}

static void
sc2_2_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		root_tx(root, pop, TEST_NITERATIONS, false, true);
		trap = 1;
		root_tx(root, pop, TEST_NITERATIONS, false, true);
	} TX_END
}

static void
sc2_012_verify_abort(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERTeq(D_RW(root)->value[0], 0);
}

static void
sc2_01_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERTeq(D_RW(root)->value[0], TEST_NITERATIONS * TEST_VALUE);
}

static void
sc2_2_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERTeq(D_RW(root)->value[0], 2 * TEST_NITERATIONS * TEST_VALUE);
}

/*
 * sc3_create
 */
static void
sc3_0_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	trap = 1;
	root_tx(root, pop, TEST_NITERATIONS, true, true);
}

static void
sc3_1_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		root_tx(root, pop, TEST_NITERATIONS, true, true);
		trap = 1;
	} TX_END
}

static void
sc3_2_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		root_tx(root, pop, TEST_NITERATIONS, true, true);
		trap = 1;
		root_tx(root, pop, TEST_NITERATIONS, true, true);
	} TX_END
}

static void
sc3_012_verify_abort(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RW(root)->value[i], 0);
}

static void
sc3_01_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RW(root)->value[i], TEST_NITERATIONS *
			TEST_VALUE);
}

static void
sc3_2_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RW(root)->value[i], 2 * TEST_NITERATIONS *
			TEST_VALUE);
}

/*
 * sc4_create
 */
static void
sc4_0_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	trap = 1;
	foo_tx(foo, pop, TEST_NITERATIONS, false, true);
}

static void
sc4_1_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	TX_BEGIN(pop) {
		foo_tx(foo, pop, TEST_NITERATIONS, false, true);
		trap = 1;
	} TX_END
}

static void
sc4_2_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	TX_BEGIN(pop) {
		foo_tx(foo, pop, TEST_NITERATIONS, false, true);
		trap = 1;
		foo_tx(foo, pop, TEST_NITERATIONS, false, true);
	} TX_END
}

static void
sc4_012_verify_abort(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	UT_ASSERTeq(D_RW(foo)->bar[0], 0);
}

static void
sc4_01_verify_commit(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	UT_ASSERTeq(D_RW(foo)->bar[0], TEST_NITERATIONS * TEST_VALUE);
}

static void
sc4_2_verify_commit(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	UT_ASSERTeq(D_RW(foo)->bar[0], 2 * TEST_NITERATIONS * TEST_VALUE);
}

/*
 * sc5_create
 */
static void
sc5_0_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);
	trap = 1;
	foo_tx(foo, pop, TEST_NITERATIONS, true, true);
}

static void
sc5_1_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	TX_BEGIN(pop) {
		foo_tx(foo, pop, TEST_NITERATIONS, true, true);
		trap = 1;
	} TX_END
}

static void
sc5_2_create(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	TX_BEGIN(pop) {
		foo_tx(foo, pop, TEST_NITERATIONS, true, true);
		trap = 1;
		foo_tx(foo, pop, TEST_NITERATIONS, true, true);
	} TX_END
}

static void
sc5_012_verify_abort(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	for (int i = 0; i < SMALL_ALLOC; ++i)
		UT_ASSERTeq(D_RW(foo)->bar[i], 0);
}

static void
sc5_01_verify_commit(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	for (int i = 0; i < SMALL_ALLOC; ++i)
		UT_ASSERTeq(D_RW(foo)->bar[i], TEST_NITERATIONS * TEST_VALUE);
}

static void
sc5_2_verify_commit(PMEMobjpool *pop)
{
	TOID(struct foo) foo = POBJ_ROOT(pop, struct foo);

	for (int i = 0; i < SMALL_ALLOC; ++i)
		UT_ASSERTeq(D_RW(foo)->bar[i], 2 * TEST_NITERATIONS *
			TEST_VALUE);
}

/*
 * sc6_create -- free undo
 */
static void
sc6_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		TX_SET(root, foo, TX_NEW(struct foo));
		TX_SET(root, bar, TX_NEW(struct bar));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	trap = 1;
	TX_BEGIN(pop) {
		TX_FREE(D_RO(root)->foo);
		TX_FREE(D_RO(root)->bar);
	} TX_END
}

static void
sc6_verify_abort(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
	/*
	 * If the free undo log didn't get unrolled then the next
	 * free would fail due to the object being already freed.
	 */
		TX_FREE(D_RO(root)->foo);
		TX_FREE(D_RO(root)->bar);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

static void
sc6_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		TOID(struct foo) f = TX_NEW(struct foo);
		UT_ASSERT(TOID_EQUALS(f, D_RO(root)->foo));
		TOID(struct bar) b = TX_NEW(struct bar);
		UT_ASSERT(TOID_EQUALS(b, D_RO(root)->bar));
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * sc7_create -- alloc undo
 */
static void
sc7_create(PMEMobjpool *pop)
{
	/* Allocate until OOM and count allocs */
	int nallocs = 0;

	TX_BEGIN(pop) {
		for (;;) {
			(void) TX_NEW(struct foo);
			nallocs++;
		}
	} TX_END

	trap = 1;
	/* Allocate all possible objects */
	TX_BEGIN(pop) {
		for (int i = 0; i < nallocs; ++i) {
			(void) TX_NEW(struct foo);
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

static void
sc7_verify_abort(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		TOID(struct foo) f = TX_NEW(struct foo);
		(void) f;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

static void
sc7_verify_commit(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		/*
		 * Due to a bug in clang-3.4 a pmemobj_tx_alloc call with its
		 * result being casted to union immediately is optimized out and
		 * the verify fails even though it should work. Assinging the
		 * TX_NEW result to a variable is a hacky workaround for this
		 * problem.
		 */
		TOID(struct foo) f = TX_NEW(struct foo);
		(void) f;
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * sc8_create
 */
static void
sc8_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	POBJ_ALLOC(pop, &D_RW(root)->bar, struct bar,
		sizeof(struct bar), NULL, NULL);
	POBJ_ALLOC(pop, &D_RW(root)->foo, struct foo,
		sizeof(struct foo), NULL, NULL);

	TX_BEGIN(pop) {
		foo_tx(D_RW(root)->foo, pop, TEST_NITERATIONS, true, true);
		bar_tx(D_RW(root)->bar, pop, TEST_NITERATIONS, true, true);
		root_tx(root, pop, TEST_NITERATIONS, true, true);
		trap = 1;
		foo_tx(D_RW(root)->foo, pop, TEST_NITERATIONS, false, true);
		bar_tx(D_RW(root)->bar, pop, TEST_NITERATIONS, false, true);
		root_tx(root, pop, TEST_NITERATIONS, false, true);
	} TX_END
}

static void
sc8_verify_abort(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	for (int i = 0; i < SMALL_ALLOC; ++i)
		UT_ASSERTeq(D_RW(D_RW(root)->foo)->bar[i], 0);
	for (int i = 0; i < BIG_ALLOC; ++i)
		UT_ASSERTeq(D_RW(D_RW(root)->bar)->foo[i], 0);
	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RW(root)->value[i], 0);
}

static void
sc8_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	for (int i = 0; i < SMALL_ALLOC; ++i)
		UT_ASSERTeq(D_RW(D_RW(root)->foo)->bar[i],
			2 * TEST_NITERATIONS * TEST_VALUE);
	for (int i = 0; i < BIG_ALLOC; ++i)
		UT_ASSERTeq(D_RW(D_RW(root)->bar)->foo[i],
			2 * TEST_NITERATIONS * TEST_VALUE);
	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RW(root)->value[i],
			2 * TEST_NITERATIONS * TEST_VALUE);
}

typedef void (*scenario_func)(PMEMobjpool *pop);

struct {
	scenario_func create;
	scenario_func verify_abort;
	scenario_func verify_commit;
} scenarios[] = {
	{sc0_create, sc0_verify_abort, sc0_verify_commit},
	{sc1_create, sc1_verify_abort, sc1_verify_commit},
	{sc2_0_create, sc2_012_verify_abort, sc2_01_verify_commit},
	{sc2_1_create, sc2_012_verify_abort, sc2_01_verify_commit},
	{sc2_2_create, sc2_012_verify_abort, sc2_2_verify_commit},
	{sc3_0_create, sc3_012_verify_abort, sc3_01_verify_commit},
	{sc3_1_create, sc3_012_verify_abort, sc3_01_verify_commit},
	{sc3_2_create, sc3_012_verify_abort, sc3_2_verify_commit},
	{sc4_0_create, sc4_012_verify_abort, sc4_01_verify_commit},
	{sc4_1_create, sc4_012_verify_abort, sc4_01_verify_commit},
	{sc4_2_create, sc4_012_verify_abort, sc4_2_verify_commit},
	{sc5_0_create, sc5_012_verify_abort, sc5_01_verify_commit},
	{sc5_1_create, sc5_012_verify_abort, sc5_01_verify_commit},
	{sc5_2_create, sc5_012_verify_abort, sc5_2_verify_commit},
	{sc6_create, sc6_verify_abort, sc6_verify_commit},
	{sc7_create, sc7_verify_abort, sc7_verify_commit},
	{sc8_create, sc8_verify_abort, sc8_verify_commit},
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_convert");

	if (argc != 4)
		UT_FATAL("usage: %s file [c|va|vc] scenario", argv[0]);

	const char *path = argv[1];
	int create_pool = argv[2][0] == 'c';
	int verify_abort = argv[2][1] == 'a';
	int sc = atoi(argv[3]);

	PMEMobjpool *pop;

	if (create_pool) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(convert),
			2 * PMEMOBJ_MIN_POOL, 0666)) == NULL) {
		UT_FATAL("failed to create pool\n");
		}
		scenarios[sc].create(pop);
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(convert)))
			== NULL) {
			UT_FATAL("failed to open pool\n");
		}
		if (verify_abort)
			scenarios[sc].verify_abort(pop);
		else
			scenarios[sc].verify_commit(pop);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
