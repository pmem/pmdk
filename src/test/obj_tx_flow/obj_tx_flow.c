/*
 * Copyright 2015-2017, Intel Corporation
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
 * obj_tx_flow.c -- unit test for transaction flow
 */
#include "unittest.h"

#define LAYOUT_NAME "direct"

#define TEST_VALUE_A 5
#define TEST_VALUE_B 10
#define TEST_VALUE_C 15
#define OPS_NUM 8
TOID_DECLARE(struct test_obj, 1);

struct test_obj {
	int a;
	int b;
	int c;
};


static void
do_tx_macro_commit(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	TX_BEGIN(pop) {
		D_RW(*obj)->a = TEST_VALUE_A;
	} TX_ONCOMMIT {
		UT_ASSERT(D_RW(*obj)->a == TEST_VALUE_A);
		D_RW(*obj)->b = TEST_VALUE_B;
	} TX_ONABORT { /* not called */
		D_RW(*obj)->a = TEST_VALUE_B;
	} TX_FINALLY {
		UT_ASSERT(D_RW(*obj)->b == TEST_VALUE_B);
		D_RW(*obj)->c = TEST_VALUE_C;
	} TX_END
}

static void
do_tx_macro_abort(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	D_RW(*obj)->a = TEST_VALUE_A;
	D_RW(*obj)->b = TEST_VALUE_B;
	TX_BEGIN(pop) {
		TX_ADD(*obj);
		D_RW(*obj)->a = TEST_VALUE_B;
		pmemobj_tx_abort(EINVAL);
		D_RW(*obj)->b = TEST_VALUE_A;
	} TX_ONCOMMIT { /* not called */
		D_RW(*obj)->a = TEST_VALUE_B;
	} TX_ONABORT {
		UT_ASSERT(D_RW(*obj)->a == TEST_VALUE_A);
		UT_ASSERT(D_RW(*obj)->b == TEST_VALUE_B);
		D_RW(*obj)->b = TEST_VALUE_B;
	} TX_FINALLY {
		UT_ASSERT(D_RW(*obj)->b == TEST_VALUE_B);
		D_RW(*obj)->c = TEST_VALUE_C;
	} TX_END
}

static void
do_tx_macro_commit_nested(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	TX_BEGIN(pop) {
		TX_BEGIN(pop) {
			D_RW(*obj)->a = TEST_VALUE_A;
		} TX_ONCOMMIT {
			UT_ASSERT(D_RW(*obj)->a == TEST_VALUE_A);
			D_RW(*obj)->b = TEST_VALUE_B;
		} TX_END
	} TX_ONCOMMIT {
		D_RW(*obj)->c = TEST_VALUE_C;
	} TX_END
}

static void
do_tx_macro_abort_nested(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	int a = 0;
	int b = 0;
	int c = 0;
	D_RW(*obj)->a = TEST_VALUE_A;
	D_RW(*obj)->b = TEST_VALUE_B;
	TX_BEGIN(pop) {
		TX_ADD(*obj);
		D_RW(*obj)->a = TEST_VALUE_B;
		a = TEST_VALUE_C;
		TX_BEGIN(pop) {
			D_RW(*obj)->b = TEST_VALUE_C;
			a = TEST_VALUE_A;
			pmemobj_tx_abort(EINVAL);
			a = TEST_VALUE_B;
		} TX_ONCOMMIT { /* not called */
			a = TEST_VALUE_C;
		} TX_ONABORT {
			UT_ASSERT(a == TEST_VALUE_A);
			b = TEST_VALUE_B;
		} TX_FINALLY {
			UT_ASSERT(b == TEST_VALUE_B);
			c = TEST_VALUE_C;
		} TX_END
		a = TEST_VALUE_B;
	} TX_ONCOMMIT { /* not called */
		UT_ASSERT(a == TEST_VALUE_A);
		c = TEST_VALUE_C;
	} TX_ONABORT {
		UT_ASSERT(a == TEST_VALUE_A);
		UT_ASSERT(b == TEST_VALUE_B);
		UT_ASSERT(c == TEST_VALUE_C);
		b = TEST_VALUE_A;
	} TX_FINALLY {
		UT_ASSERT(b == TEST_VALUE_A);
		D_RW(*obj)->c = TEST_VALUE_C;
		a = TEST_VALUE_B;
	} TX_END
	UT_ASSERT(a == TEST_VALUE_B);
}

static void
do_tx_commit(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{

	pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	D_RW(*obj)->a = TEST_VALUE_A;
	TX_ADD(*obj);
	D_RW(*obj)->b = TEST_VALUE_B;
	pmemobj_tx_commit();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONCOMMIT);
	D_RW(*obj)->c = TEST_VALUE_C;
	pmemobj_tx_end();
}

static void
do_tx_commit_nested(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{

	pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	TX_ADD(*obj);
	D_RW(*obj)->a = TEST_VALUE_A;
		pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
		TX_ADD(*obj);
		D_RW(*obj)->b = TEST_VALUE_B;
		pmemobj_tx_commit();
		UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONCOMMIT);
		pmemobj_tx_end();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_WORK);
	pmemobj_tx_commit();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONCOMMIT);
	D_RW(*obj)->c = TEST_VALUE_C;
	pmemobj_tx_end();
}

static void
do_tx_abort(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	D_RW(*obj)->a = TEST_VALUE_A;
	pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	D_RW(*obj)->b = TEST_VALUE_B;
	TX_ADD(*obj);
	D_RW(*obj)->a = 0;
	pmemobj_tx_abort(EINVAL);
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONABORT);
	D_RW(*obj)->c = TEST_VALUE_C;
	pmemobj_tx_end();
}

static void
do_tx_abort_nested(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	D_RW(*obj)->a = TEST_VALUE_A;
	D_RW(*obj)->b = TEST_VALUE_B;
	pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	TX_ADD(*obj);
	D_RW(*obj)->a = 0;
		pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
		TX_ADD(*obj);
		D_RW(*obj)->b = 0;
		pmemobj_tx_abort(EINVAL);
		UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONABORT);
		pmemobj_tx_end();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONABORT);
	D_RW(*obj)->c = TEST_VALUE_C;
	pmemobj_tx_end();
}

typedef void (*fn_op)(PMEMobjpool *pop, TOID(struct test_obj) *obj);
static fn_op tx_op[OPS_NUM] = {do_tx_macro_commit, do_tx_macro_abort,
			do_tx_macro_commit_nested, do_tx_macro_abort_nested,
			do_tx_commit, do_tx_commit_nested, do_tx_abort,
			do_tx_abort_nested};

static void
do_tx_process(PMEMobjpool *pop)
{
	pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_WORK);
	pmemobj_tx_process();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONCOMMIT);
	pmemobj_tx_process();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_FINALLY);
	pmemobj_tx_process();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_NONE);
	pmemobj_tx_end();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_NONE);
}

static void
do_tx_process_nested(PMEMobjpool *pop)
{
	pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_WORK);
		pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
		pmemobj_tx_process();
		UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONCOMMIT);
		pmemobj_tx_process();
		UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_FINALLY);
		pmemobj_tx_end();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_WORK);
	pmemobj_tx_abort(EINVAL);
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_ONABORT);
	pmemobj_tx_process();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_FINALLY);
	pmemobj_tx_process();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_NONE);
	pmemobj_tx_end();
	UT_ASSERT(pmemobj_tx_stage() == TX_STAGE_NONE);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_flow");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	TOID(struct test_obj) obj;
	POBJ_ZNEW(pop, &obj, struct test_obj);

	for (int i = 0; i < OPS_NUM; i++) {
		D_RW(obj)->a = 0;
		D_RW(obj)->b = 0;
		D_RW(obj)->c = 0;
		tx_op[i](pop, &obj);

		UT_ASSERT(D_RO(obj)->a == TEST_VALUE_A);
		UT_ASSERT(D_RO(obj)->b == TEST_VALUE_B);
		UT_ASSERT(D_RO(obj)->c == TEST_VALUE_C);
	}
	do_tx_process(pop);
	do_tx_process_nested(pop);
	pmemobj_close(pop);

	DONE(NULL);
}
