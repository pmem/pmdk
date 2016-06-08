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
 * functions and to verify if the conversion happened correctly.
 *
 * The creation should happen while linked with the old library version and
 * the verify step should be run with the new one.
 */
#include "libpmemobj.h"
#include "unittest.h"

POBJ_LAYOUT_BEGIN(convert);
POBJ_LAYOUT_ROOT(convert, struct root);
POBJ_LAYOUT_TOID(convert, struct foo);
POBJ_LAYOUT_TOID(convert, struct bar);
POBJ_LAYOUT_END(convert);

#define SMALL_ALLOC (64)
#define BIG_ALLOC (1024 * 200) /* just big enough to be a huge allocation */

struct bar {
	char foo[BIG_ALLOC];
};

struct foo {
	unsigned char bar[SMALL_ALLOC];
};

#define TEST_VALUE 5
#define TEST_NVALUES 10

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
 * sc1_create -- cache set undo
 */
static void
sc1_create(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	trap = 1;
	TX_BEGIN(pop) {
		for (int i = 0; i < TEST_NVALUES; ++i)
			TX_SET(root, value[i], TEST_VALUE);
	} TX_END
}

static void
sc1_verify_abort(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RO(root)->value[i], 0);
}

static void
sc1_verify_commit(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	for (int i = 0; i < TEST_NVALUES; ++i)
		UT_ASSERTeq(D_RO(root)->value[i], TEST_VALUE);
}

/*
 * sc2_create -- free undo
 */
static void
sc2_create(PMEMobjpool *pop)
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
sc2_verify_abort(PMEMobjpool *pop)
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
sc2_verify_commit(PMEMobjpool *pop)
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
 * sc3_create -- alloc undo
 */
static void
sc3_create(PMEMobjpool *pop)
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
	/* allocate all possible objects */
	TX_BEGIN(pop) {
		for (int i = 0; i < nallocs; ++i) {
			(void) TX_NEW(struct foo);
		}
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

static void
sc3_verify_abort(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		TOID(struct foo) f = TX_NEW(struct foo);
		(void) f;
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

static void
sc3_verify_commit(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		/*
		 * Due to a bug in clang-3.4 a pmemobj_tx_alloc call with its
		 * result being casted to union immediately is optimized out and
		 * the verify fails even though it should work. Assigning the
		 * TX_NEW result to a variable is a hacky workaround for this
		 * problem.
		 */
		TOID(struct foo) f = TX_NEW(struct foo);
		(void) f;
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_END
}

typedef void (*scenario_func)(PMEMobjpool *pop);

struct {
	scenario_func create;
	scenario_func verify_abort;
	scenario_func verify_commit;
} scenarios[] = {
	{sc0_create, sc0_verify_abort, sc0_verify_commit},
	{sc1_create, sc1_verify_abort, sc1_verify_commit},
	{sc2_create, sc2_verify_abort, sc2_verify_commit},
	{sc3_create, sc3_verify_abort, sc3_verify_commit},
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_convert");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(convert) != 2);

	if (argc != 4)
		UT_FATAL("usage: %s file [c|va|vc] scenario", argv[0]);

	const char *path = argv[1];
	int create_pool = argv[2][0] == 'c';
	int verify_abort = argv[2][1] == 'a';
	int sc = atoi(argv[3]);

	PMEMobjpool *pop;

	if (create_pool) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(convert),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
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
