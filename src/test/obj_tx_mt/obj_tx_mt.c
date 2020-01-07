// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * obj_tx_mt.c -- multi-threaded test for pmemobj_tx_*
 *
 * It checks that objects are removed from transactions before on abort/commit
 * phase.
 */
#include "unittest.h"
#include "sys_util.h"

#define THREADS 8
#define LOOPS 8

static PMEMobjpool *pop;
static PMEMoid tab;
static os_mutex_t mtx;

static void *
tx_alloc_free(void *arg)
{
	volatile int locked;
	for (int i = 0; i < LOOPS; ++i) {
		locked = 0;
		TX_BEGIN(pop) {
			util_mutex_lock(&mtx);
			locked = 1;
			tab = pmemobj_tx_zalloc(128, 1);
		} TX_ONCOMMIT {
			if (locked)
				util_mutex_unlock(&mtx);
		} TX_ONABORT {
			if (locked)
				util_mutex_unlock(&mtx);
		} TX_END
		locked = 0;
		TX_BEGIN(pop) {
			util_mutex_lock(&mtx);
			locked = 1;
			pmemobj_tx_free(tab);
			tab = OID_NULL;
		} TX_ONCOMMIT {
			if (locked)
				util_mutex_unlock(&mtx);
		} TX_ONABORT {
			if (locked)
				util_mutex_unlock(&mtx);
		} TX_END
	}
	return NULL;
}

static void *
tx_snap(void *arg)
{
	volatile int locked;
	for (int i = 0; i < LOOPS; ++i) {
		locked = 0;
		TX_BEGIN(pop) {
			util_mutex_lock(&mtx);
			locked = 1;
			if (!OID_IS_NULL(tab))
				pmemobj_tx_add_range(tab, 0, 8);
		} TX_ONCOMMIT {
			if (locked)
				util_mutex_unlock(&mtx);
		} TX_ONABORT {
			if (locked)
				util_mutex_unlock(&mtx);
		} TX_END
		locked = 0;
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_mt");

	util_mutex_init(&mtx);

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	if ((pop = pmemobj_create(argv[1], "mt", PMEMOBJ_MIN_POOL,
			S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	int i = 0;
	os_thread_t *threads = MALLOC(THREADS * sizeof(threads[0]));

	for (int j = 0; j < THREADS / 2; ++j) {
		THREAD_CREATE(&threads[i++], NULL, tx_alloc_free, NULL);
		THREAD_CREATE(&threads[i++], NULL, tx_snap, NULL);
	}

	while (i > 0)
		THREAD_JOIN(&threads[--i], NULL);

	pmemobj_close(pop);

	util_mutex_destroy(&mtx);

	FREE(threads);

	DONE(NULL);
}
