// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * obj_tx_callbacks.c -- unit test for transaction stage callbacks
 */
#include "unittest.h"

#define LAYOUT_NAME "tx_callback"

POBJ_LAYOUT_BEGIN(tx_callback);
POBJ_LAYOUT_ROOT(tx_callback, struct pmem_root);
POBJ_LAYOUT_TOID(tx_callback, struct pmem_obj);
POBJ_LAYOUT_END(tx_callback);

struct runtime_info {
	int something;
};

struct pmem_obj {
	struct runtime_info *rt;
	int pmem_info;
};

struct pmem_root {
	TOID(struct pmem_obj) obj;
};

struct free_info {
	void *to_free;
};

static int freed = 0;

static const char *desc[] = {
	"TX_STAGE_NONE",
	"TX_STAGE_WORK",
	"TX_STAGE_ONCOMMIT",
	"TX_STAGE_ONABORT",
	"TX_STAGE_FINALLY",
	"WTF?"
};

static void
free_onabort(PMEMobjpool *pop, enum pobj_tx_stage stage, void *arg)
{
	UT_OUT("cb stage: %s", desc[stage]);
	if (stage == TX_STAGE_ONABORT) {
		struct free_info *f = (struct free_info *)arg;
		UT_OUT("rt_onabort: free");
		free(f->to_free);
		freed++;
	}
}

static void
allocate_pmem(struct free_info *f, TOID(struct pmem_root) root, int val)
{
	TOID(struct pmem_obj) obj = TX_NEW(struct pmem_obj);
	D_RW(obj)->pmem_info = val;
	D_RW(obj)->rt =
		(struct runtime_info *)malloc(sizeof(struct runtime_info));

	f->to_free = D_RW(obj)->rt;

	D_RW(obj)->rt->something = val;
	TX_ADD_FIELD(root, obj);
	D_RW(root)->obj = obj;
}

static void
do_something_fishy(TOID(struct pmem_root) root)
{
	TX_ADD_FIELD(root, obj);
	D_RW(root)->obj = TX_ALLOC(struct pmem_obj, 1 << 30);
}

static void
free_oncommit(PMEMobjpool *pop, enum pobj_tx_stage stage, void *arg)
{
	UT_OUT("cb stage: %s", desc[stage]);
	if (stage == TX_STAGE_ONCOMMIT) {
		struct free_info *f = (struct free_info *)arg;
		UT_OUT("rt_oncommit: free");
		free(f->to_free);
		freed++;
	}
}

static void
free_pmem(struct free_info *f, TOID(struct pmem_root) root)
{
	TOID(struct pmem_obj) obj = D_RW(root)->obj;
	f->to_free = D_RW(obj)->rt;
	TX_FREE(obj);
	TX_SET(root, obj, TOID_NULL(struct pmem_obj));
}

static void
log_stages(PMEMobjpool *pop, enum pobj_tx_stage stage, void *arg)
{
	UT_OUT("cb stage: %s", desc[stage]);
}

static void
test(PMEMobjpool *pop, TOID(struct pmem_root) root)
{
	struct free_info *volatile f = (struct free_info *)ZALLOC(sizeof(*f));
	TX_BEGIN_CB(pop, free_onabort, f) {
		allocate_pmem(f, root, 7);
		do_something_fishy(root);
		UT_ASSERT(0);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		UT_OUT("on abort 1");
	} TX_FINALLY {
		UT_OUT("finally 1");
	} TX_END
	UT_OUT("end of tx 1\n");
	memset(f, 0, sizeof(*f));

	UT_ASSERTeq(freed, 1);
	freed = 0;

	TX_BEGIN_CB(pop, free_onabort, f) {
		allocate_pmem(f, root, 7);
	} TX_ONCOMMIT {
		UT_OUT("on commit 2");
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_FINALLY {
		UT_OUT("finally 2");
	} TX_END
	UT_OUT("end of tx 2\n");
	memset(f, 0, sizeof(*f));

	UT_ASSERTeq(freed, 0);

	TX_BEGIN_CB(pop, free_oncommit, f) {
		free_pmem(f, root);
		do_something_fishy(root);
		UT_ASSERT(0);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		UT_OUT("on abort 3");
	} TX_FINALLY {
		UT_OUT("finally 3");
	} TX_END
	UT_OUT("end of tx 3\n");
	memset(f, 0, sizeof(*f));

	UT_ASSERTeq(freed, 0);

	TX_BEGIN_CB(pop, free_oncommit, f) {
		free_pmem(f, root);
	} TX_ONCOMMIT {
		UT_OUT("on commit 4");
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_FINALLY {
		UT_OUT("finally 4");
	} TX_END
	UT_OUT("end of tx 4\n");
	memset(f, 0, sizeof(*f));

	UT_ASSERTeq(freed, 1);
	freed = 0;

	TX_BEGIN_CB(pop, log_stages, NULL) {
		TX_BEGIN(pop) {
			UT_OUT("inner tx work 5");
		} TX_ONCOMMIT {
			UT_OUT("inner tx on commit 5");
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_FINALLY {
			UT_OUT("inner tx finally 5");
		} TX_END
	} TX_ONCOMMIT {
		UT_OUT("on commit 5");
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_FINALLY {
		UT_OUT("finally 5");
	} TX_END
	UT_OUT("end of tx 5\n");

	TX_BEGIN(pop) {
		TX_BEGIN_CB(pop, log_stages, NULL) {
			UT_OUT("inner tx work 6");
		} TX_ONCOMMIT {
			UT_OUT("inner tx on commit 6");
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_FINALLY {
			UT_OUT("inner tx finally 6");
		} TX_END
	} TX_ONCOMMIT {
		UT_OUT("on commit 6");
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_FINALLY {
		UT_OUT("finally 6");
	} TX_END
	UT_OUT("end of tx 6\n");

	UT_OUT("start of tx 7");
	if (pmemobj_tx_begin(pop, NULL, TX_PARAM_CB, log_stages, NULL,
			TX_PARAM_NONE))
		UT_FATAL("!pmemobj_tx_begin");
	UT_OUT("work");
	pmemobj_tx_commit();
	UT_OUT("on commit");
	if (pmemobj_tx_end())
		UT_FATAL("!pmemobj_tx_end");
	UT_OUT("end of tx 7\n");

	FREE(f);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_callbacks");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop = pmemobj_create(argv[1], LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	if (!pop)
		UT_FATAL("!pmemobj_create");

	TOID(struct pmem_root) root = POBJ_ROOT(pop, struct pmem_root);
	test(pop, root);

	pmemobj_close(pop);

	DONE(NULL);
}
