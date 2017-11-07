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
 * obj_tx_ctx_mt.c -- a multithreaded test for changing transaction context
 *
 */

#include <stddef.h>

#include "unittest.h"

#define THREADS 8
#define LOOPS 8

static PMEMobjpool *popA;
static PMEMobjpool *popB;
static os_mutex_t mtxA;
static os_mutex_t mtxB;

struct vector {
	int x;
	int y;
	int z;
};

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
print_vectors()
{
	print_vector(popA);
	print_vector(popB);
}

static void
tx_inner()
{
	PMEMoid rootB = pmemobj_root(popB, sizeof(int));
	struct vector *v = (struct vector *)pmemobj_direct(rootB);

	struct pobj_tx_ctx *new_ctx = pmemobj_tx_ctx_new();
	struct pobj_tx_ctx *old_ctx;
	pmemobj_tx_ctx_set(new_ctx, &old_ctx);

	volatile int locked = 0;
	TX_BEGIN(popB) {
		os_mutex_lock(&mtxB);
		locked = 1;
		pmemobj_tx_add_range(rootB, 0, sizeof(struct vector));
		v->z += 1;
	} TX_ONCOMMIT {
		if (locked)
			os_mutex_unlock(&mtxB);
	} TX_ONABORT {
		if (locked)
			os_mutex_unlock(&mtxB);
	} TX_END
	pmemobj_tx_ctx_set(old_ctx, NULL);
	pmemobj_tx_ctx_delete(new_ctx);
}

static void
tx_outer_work()
{
	PMEMoid rootA = pmemobj_root(popA, sizeof(struct vector));
	struct vector *v = (struct vector *)pmemobj_direct(rootA);

	volatile int locked = 0;
	TX_BEGIN(popA) {
		os_mutex_lock(&mtxA);
		locked = 1;
		pmemobj_tx_add_range(rootA, 0, sizeof(struct vector));
		v->x += 1;
		tx_inner();
	} TX_ONCOMMIT {
		if (locked)
			os_mutex_unlock(&mtxA);
	} TX_ONABORT {
		if (locked)
			os_mutex_unlock(&mtxA);
	} TX_END
}

static void
tx_outer_abort()
{
	PMEMoid rootA = pmemobj_root(popA, sizeof(struct vector));
	struct vector *v = (struct vector *)pmemobj_direct(rootA);

	volatile int locked = 0;
	TX_BEGIN(popA) {
		os_mutex_lock(&mtxA);
		locked = 1;
		pmemobj_tx_add_range(rootA, 0, sizeof(struct vector));
		v->y += 1;
		pmemobj_tx_abort(ECANCELED);
	} TX_ONCOMMIT {
		if (locked)
			os_mutex_unlock(&mtxA);
	} TX_ONABORT {
		tx_inner();
		if (locked)
			os_mutex_unlock(&mtxA);
	} TX_END
}

static void *
tx_nest_work(void *arg)
{
	for (int i = 0; i < LOOPS; ++i) {
		tx_outer_work();
	}
	return NULL;
}

static void *
tx_nest_abort(void *arg)
{
	for (int i = 0; i < LOOPS; ++i) {
		tx_outer_abort();
	}
	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_ctx_mt");

	if (argc != 3)
		UT_FATAL("usage: %s file-name-A file-name-B", argv[0]);

	const char *pathA = argv[1];
	const char *pathB = argv[2];

	/* create and open the pools */
	if ((popA = pmemobj_create(pathA, "A", 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", pathA);

	if ((popB = pmemobj_create(pathB, "B", 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", pathB);

	/* execute testcases with changing the transaction context */
	zero_vectors();

	os_mutex_init(&mtxA);
	os_mutex_init(&mtxB);

	int i = 0;
	os_thread_t *threads =
		(os_thread_t *)MALLOC(THREADS * sizeof(threads[0]));

	for (int j = 0; j < THREADS / 2; ++j) {
		PTHREAD_CREATE(&threads[i++], NULL, tx_nest_work, NULL);
		PTHREAD_CREATE(&threads[i++], NULL, tx_nest_abort, NULL);
	}

	while (i > 0)
		PTHREAD_JOIN(threads[--i], NULL);

	print_vectors();

	pmemobj_close(popA);
	pmemobj_close(popB);

	os_mutex_destroy(&mtxA);
	os_mutex_destroy(&mtxB);

	FREE(threads);

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
