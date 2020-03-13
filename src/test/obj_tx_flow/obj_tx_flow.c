// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * obj_tx_flow.c -- unit test for transaction flow
 */
#include "unittest.h"
#include "obj.h"

#define LAYOUT_NAME "direct"

#define TEST_VALUE_A 5
#define TEST_VALUE_B 10
#define TEST_VALUE_C 15
#define OPS_NUM 9
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
	volatile int a = 0;
	volatile int b = 0;
	volatile int c = 0;
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
do_tx_macro_abort_nested_begin(PMEMobjpool *pop, TOID(struct test_obj) *obj)
{
	errno = 0;
	TX_BEGIN(pop) {
		D_RW(*obj)->a = TEST_VALUE_A;
		D_RW(*obj)->b = TEST_VALUE_B;

		pmemobj_tx_set_failure_behavior(POBJ_TX_FAILURE_RETURN);
		TX_BEGIN((PMEMobjpool *)(uintptr_t)7) {
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
		UT_ASSERT(errno == EINVAL);
	} TX_ONABORT {
		D_RW(*obj)->c = TEST_VALUE_C;
	} TX_ONCOMMIT { /* not called */
		D_RW(*obj)->a = TEST_VALUE_B;
	} TX_END
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
			do_tx_macro_abort_nested_begin, do_tx_commit,
			do_tx_commit_nested, do_tx_abort, do_tx_abort_nested};

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

static void
do_fault_injection(PMEMobjpool *pop)
{
	if (!pmemobj_fault_injection_enabled())
		return;
	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "pmemobj_tx_begin");
	int ret = pmemobj_tx_begin(pop, NULL, TX_PARAM_NONE);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_flow");

	if (argc != 3)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[2], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
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
	switch (argv[1][0]) {
	case 't':
		do_tx_process(pop);
		do_tx_process_nested(pop);
		break;
	case 'f':
		do_fault_injection(pop);
		break;
	default:
		UT_FATAL("usage: %s [t|f]", argv[0]);
	}
	pmemobj_close(pop);

	DONE(NULL);
}
