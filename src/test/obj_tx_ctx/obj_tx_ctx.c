/*
 * Copyright 2017, Intel Corporation
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
 * obj_tx_ctx.c -- a test for changing transaction context
 *
 */

#include <stddef.h>

#include "unittest.h"

struct vector {
	int x;
	int y;
	int z;
};

static PMEMobjpool *popA;
static PMEMobjpool *popB;

static void
zero_vector(PMEMobjpool *pop)
{
	PMEMoid root = pmemobj_root(pop, sizeof(struct vector));
	struct vector *v = (struct vector *)pmemobj_direct(root);

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(root, 0, sizeof(struct vector));
		v->x = 0;
		v->y = 0;
		v->z = 0;
	} TX_END
}

static void
zero_vectors(void)
{
	zero_vector(popA);
	zero_vector(popB);
}

static void
print_vector(PMEMobjpool *pop)
{
	PMEMoid root = pmemobj_root(pop, sizeof(struct vector));
	struct vector *v = (struct vector *)pmemobj_direct(root);
	UT_OUT("x = %d, y = %d, z = %d", v->x, v->y, v->z);
}

static void
print_vectors(const char *msg)
{
	UT_OUT("%s:", msg);
	print_vector(popA);
	print_vector(popB);
}

static void
store_z(PMEMobjpool *pop, int change_ctx, int abort)
{
	PMEMoid root = pmemobj_root(pop, sizeof(int));
	struct vector *v = (struct vector *)pmemobj_direct(root);
	struct pobj_tx_ctx *new_ctx;
	struct pobj_tx_ctx *old_ctx;

	if (change_ctx) {
		new_ctx = pmemobj_tx_ctx_new();
		pmemobj_tx_ctx_set(new_ctx, &old_ctx);
	}

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(root, 0, sizeof(struct vector));
		v->z = 3;
		if (abort)
			pmemobj_tx_abort(ECANCELED);
	} TX_END

	if (change_ctx) {
		pmemobj_tx_ctx_set(old_ctx, NULL);
		pmemobj_tx_ctx_delete(new_ctx);
	}
}

static const char *
stage_str(enum pobj_tx_stage stage)
{
	switch (stage) {
	case TX_STAGE_NONE:
		return "none";
		break;
	case TX_STAGE_WORK:
		return "work";
		break;
	case TX_STAGE_ONCOMMIT:
		return "oncommit";
		break;
	case TX_STAGE_ONABORT:
		return "onabort";
		break;
	case TX_STAGE_FINALLY:
		return "finally";
		break;
	default:
		return "unknown";
	}
}

static const char *
tag(PMEMobjpool *pop)
{
	if (pop == popA)
		return "A";
	else if (pop == popB)
		return "B";
	else
		return "?";
}

static void
testcase(PMEMobjpool *pop1, PMEMobjpool *pop2, enum pobj_tx_stage stage,
		int change_ctx, int abort)
{
	PMEMoid rootA = pmemobj_root(popA, sizeof(struct vector));
	struct vector *vectorA = (struct vector *)pmemobj_direct(rootA);

	errno = 0;
	zero_vectors();

	TX_BEGIN(pop1) {
		pmemobj_tx_add_range(rootA, 0, sizeof(struct vector));
		vectorA->x = 1;
		if (stage == TX_STAGE_WORK)
			store_z(pop2, change_ctx, abort);
		vectorA->y = 2;
		if (stage == TX_STAGE_ONABORT)
			pmemobj_tx_abort(ECANCELED);
	} TX_ONCOMMIT {
		if (stage == TX_STAGE_ONCOMMIT)
			store_z(pop2, change_ctx, abort);
	} TX_ONABORT {
		if (stage == TX_STAGE_ONABORT)
			store_z(pop2, change_ctx, abort);

	} TX_FINALLY {
		if (stage == TX_STAGE_FINALLY)
			store_z(pop2, change_ctx, abort);
	} TX_END

	/* assemble a label */
	char msg[30];
	strcpy(msg, stage_str(stage));
	strcat(msg, tag(pop1));
	if (change_ctx)
		strcat(msg, "ctx");
	strcat(msg, tag(pop2));
	if (abort)
		strcat(msg, "aborted");

	print_vectors(msg);

	if (errno)
		UT_OUT("%s", pmemobj_errormsg());
}

static void
test_tx_ctx(void)
{
	/* change the context without outer transaction */
	errno = 0;
	zero_vectors();
	store_z(popA, 1, 0);
	print_vectors("noneA");
	if (errno)
		UT_OUT("%s", pmemobj_errormsg());

	/* change the context without outer transaction, then abort */
	errno = 0;
	zero_vectors();
	store_z(popA, 1, 1);
	print_vectors("noneAaborted");
	if (errno)
		UT_OUT("%s", pmemobj_errormsg());

	/*
	 * nest a transaction on the same pool, when in the WORK stage,
	 * without changing the context
	 */
	testcase(popA, popA, TX_STAGE_WORK, 0, 0);

	/*
	 * nest a transaction on the same pool, when in the WORK stage,
	 * without changing the context, abort the inner transaction
	 */
	testcase(popA, popA, TX_STAGE_WORK, 0, 1);

	/*
	 * nest a transaction on the same pool, when in the WORK stage,
	 * with a context change
	 */
	testcase(popA, popA, TX_STAGE_WORK, 1, 0);

	/*
	 * nest a transaction on the same pool, when in the WORK stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popA, TX_STAGE_WORK, 1, 1);

	/*
	 * nest a transaction on a different pool, when in the WORK stage,
	 * without changing the context
	 */
	testcase(popA, popB, TX_STAGE_WORK, 0, 0);

	/*
	 * nest a transaction on a different pool, when in the WORK stage,
	 * without changing the context, abort the inner transaction
	 */
	testcase(popA, popB, TX_STAGE_WORK, 0, 1);

	/*
	 * nest a transaction on a different pool, when in the WORK stage,
	 * with a context change
	 */
	testcase(popA, popB, TX_STAGE_WORK, 1, 0);

	/*
	 * nest a transaction on a different pool, when in the WORK stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popB, TX_STAGE_WORK, 1, 1);

	/*
	 * nest a transaction on the same pool, when in the ONABORT stage,
	 * with a context change
	 */
	testcase(popA, popA, TX_STAGE_ONABORT, 1, 0);

	/*
	 * nest a transaction on the same pool, when in the ONABORT stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popA, TX_STAGE_ONABORT, 1, 1);

	/*
	 * nest a transaction on a different pool, when in the ONABORT stage,
	 * with a context change
	 */
	testcase(popA, popB, TX_STAGE_ONABORT, 1, 0);

	/*
	 * nest a transaction on a different pool, when in the ONABORT stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popB, TX_STAGE_ONABORT, 1, 1);

	/*
	 * nest a transaction on the same pool, when in the ONCOMMIT stage,
	 * with a context change
	 */
	testcase(popA, popA, TX_STAGE_ONCOMMIT, 1, 0);

	/*
	 * nest a transaction on the same pool, when in the ONCOMMIT stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popA, TX_STAGE_ONCOMMIT, 1, 1);

	/*
	 * nest a transaction on a different pool, when in the ONCOMMIT stage,
	 * with a context change
	 */
	testcase(popA, popB, TX_STAGE_ONCOMMIT, 1, 0);

	/*
	 * nest a transaction on a different pool, when in the ONCOMMIT stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popB, TX_STAGE_ONCOMMIT, 1, 1);

	/*
	 * nest a transaction on the same pool, when in the FINALLY stage,
	 * with a context change
	 */
	testcase(popA, popA, TX_STAGE_FINALLY, 1, 0);

	/*
	 * nest a transaction on the same pool, when in the FINALLY stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popA, TX_STAGE_FINALLY, 1, 1);

	/*
	 * nest a transaction on a different pool, when in the FINALLY stage,
	 * with a context change
	 */
	testcase(popA, popB, TX_STAGE_FINALLY, 1, 0);

	/*
	 * nest a transaction on a different pool, when in the FINALLY stage,
	 * with a context change, abort the inner transaction
	 */
	testcase(popA, popB, TX_STAGE_FINALLY, 1, 1);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_ctx");

	if (argc != 3)
		UT_FATAL("usage: %s file-name-A file-name-B", argv[0]);

	const char *pathA = argv[1];
	const char *pathB = argv[2];

	/* create and open the pools */
	if ((popA = pmemobj_create(pathA, "A", 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", pathA);

	if ((popB = pmemobj_create(pathB, "B", 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", pathB);

	/* execute testcases with changing transaction context */
	test_tx_ctx();

	pmemobj_close(popA);
	pmemobj_close(popB);

	/* verify the pools consistency */
	if ((popA = pmemobj_open(pathA, "A")) == NULL)
		UT_FATAL("!pmemobj_open: %s", pathA);
	pmemobj_close(popA);

	int result = pmemobj_check(pathA, "A");
	if (result < 0)
		UT_OUT("!%s: pmemobj_check", pathA);
	else if (result == 0)
		UT_OUT("%s: pmemobj_check: not consistent", pathA);

	if ((popB = pmemobj_open(pathB, "B")) == NULL)
		UT_FATAL("!pmemobj_open: %s", pathB);
	pmemobj_close(popB);

	result = pmemobj_check(pathB, "B");
	if (result < 0)
		UT_OUT("!%s: pmemobj_check", pathB);
	else if (result == 0)
		UT_OUT("%s: pmemobj_check: not consistent", pathB);

	DONE(NULL);
}
