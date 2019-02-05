/*
 * Copyright 2019, Intel Corporation
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
 * obj_ctl_arenas.c -- tests for the ctl entry points
 * usage:
 * obj_ctl_arenas <file> n - test for heap.narenas.total
 * obj_ctl_arenas <file> s - test for heap.arena.[idx].size
 * and heap.thread.arena_id (RW)
 * obj_ctl_arenas <file> c - test for heap.arena.create,
 * heap.arena.[idx].automatic and heap.narenas.automatic
 */

#include <sched.h>
#include "sys_util.h"
#include "unittest.h"
#include "util.h"

#define CHUNKSIZE ((size_t)1024 * 256)	/* 256 kilobytes */
#define LAYOUT "obj_ctl_arenas"
#define CTL_QUERY_LEN 256
#define NTHREAD 2

static os_mutex_t lock;
static os_cond_t cond;

static PMEMobjpool *pop;
static int nth;
static struct pobj_alloc_class_desc alloc_class[] = {
	{
		.header_type = POBJ_HEADER_NONE,
		.unit_size = 128,
		.units_per_block = 1000,
		.alignment = 0
	},
	{
		.header_type = POBJ_HEADER_NONE,
		.unit_size = 1024,
		.units_per_block = 1000,
		.alignment = 0
	},
};

static void *
worker_arenas_size(void *arg)
{
	int ret = -1;
	int idx = (int)(intptr_t)arg;
	int off_idx = idx + 128;
	unsigned arena_id;
	unsigned arena_id_new;
	size_t arena_size;
	char arena_idx_size[CTL_QUERY_LEN];
	char alloc_class_idx_desc[CTL_QUERY_LEN];

	ret = pmemobj_ctl_exec(pop, "heap.arena.create",
			&arena_id_new);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(arena_id_new > 1);

	ret = pmemobj_ctl_set(pop, "heap.thread.arena_id",
			&arena_id_new);
	UT_ASSERTeq(ret, 0);

	ret = snprintf(alloc_class_idx_desc, CTL_QUERY_LEN,
			"heap.alloc_class.%d.desc", off_idx);
	if (ret < 0 || ret >= CTL_QUERY_LEN)
		UT_FATAL("!snprintf alloc_class_idx_desc");

	ret = pmemobj_ctl_set(pop, alloc_class_idx_desc, &alloc_class[idx]);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_xalloc(pop, NULL, alloc_class[idx].unit_size, 0,
			POBJ_CLASS_ID(off_idx), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	/* we need to test 2 arenas so 2 threads are needed here */
	util_mutex_lock(&lock);
	nth++;
	if (nth == NTHREAD)
		os_cond_broadcast(&cond);
	else
		while (nth < NTHREAD)
			os_cond_wait(&cond, &lock);
	util_mutex_unlock(&lock);

	ret = pmemobj_ctl_get(pop, "heap.thread.arena_id", &arena_id);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arena_id_new, arena_id);

	ret = snprintf(arena_idx_size, CTL_QUERY_LEN,
			"heap.arena.%u.size", arena_id);
	if (ret < 0 || ret >= CTL_QUERY_LEN)
		UT_FATAL("!snprintf arena_idx_size");

	ret = pmemobj_ctl_get(pop, arena_idx_size, &arena_size);
	UT_ASSERTeq(ret, 0);

	size_t test = ALIGN_UP(alloc_class[idx].unit_size *
			alloc_class[idx].units_per_block, CHUNKSIZE);
	UT_ASSERTeq(test, arena_size);

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_arenas");

	if (argc != 3)
		UT_FATAL("usage: %s poolset [n]", argv[0]);

	const char *path = argv[1];
	char t = argv[2][0];

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL * 20,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	int ret = 0;

	if (t == 'n') {
		unsigned narenas = 0;
		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &narenas);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTne(narenas, 0);
	} else if (t == 's') {
		os_thread_t threads[NTHREAD];
		util_mutex_init(&lock);
		os_cond_init(&cond);

		for (int i = 0; i < NTHREAD; i++)
			PTHREAD_CREATE(&threads[i], NULL, worker_arenas_size,
					(void *)(intptr_t)i);

		for (int i = 0; i < NTHREAD; i++)
			PTHREAD_JOIN(&threads[i], NULL);

		PMEMoid oid, oid2;
		POBJ_FOREACH_SAFE(pop, oid, oid2)
			pmemobj_free(&oid);

		util_mutex_destroy(&lock);
		os_cond_destroy(&cond);
	} else if (t == 'c') {
		char arena_idx_auto[CTL_QUERY_LEN];
		unsigned narenas_b = 0;
		unsigned narenas_a = 0;
		unsigned narenas_n = 4;
		unsigned arena_id;
		unsigned all_auto;
		int automatic;

		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &narenas_b);
		UT_ASSERTeq(ret, 0);

		/* all arenas created at the start should be set to auto  */
		for (unsigned i = 0; i < narenas_b; i++) {
			ret = snprintf(arena_idx_auto, CTL_QUERY_LEN,
					"heap.arena.%u.automatic", i);
			if (ret < 0 || ret >= CTL_QUERY_LEN)
				UT_FATAL("!snprintf arena_idx_auto");

			ret = pmemobj_ctl_get(pop, arena_idx_auto, &automatic);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(automatic, 1);
		}
		ret = pmemobj_ctl_get(pop, "heap.narenas.automatic", &all_auto);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(narenas_b, all_auto);

		/* all arenas created by user should not be auto  */
		for (unsigned i = 0; i < narenas_n; i++) {
			ret = pmemobj_ctl_exec(pop, "heap.arena.create",
					&arena_id);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(arena_id, narenas_b + i);

			ret = snprintf(arena_idx_auto, CTL_QUERY_LEN,
					"heap.arena.%u.automatic", arena_id);
			if (ret < 0 || ret >= CTL_QUERY_LEN)
				UT_FATAL("!snprintf arena_idx_auto");
			ret = pmemobj_ctl_get(pop, arena_idx_auto, &automatic);
			UT_ASSERTeq(automatic, 0);

			/*
			 * after creation, number of auto
			 * arenas should be the same
			 */
			ret = pmemobj_ctl_get(pop, "heap.narenas.automatic",
					&all_auto);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(narenas_b + i, all_auto);

			/* change the state of created arena to auto */
			int activate = 1;
			ret = pmemobj_ctl_set(pop, arena_idx_auto,
					&activate);
			UT_ASSERTeq(ret, 0);
			ret = pmemobj_ctl_get(pop, arena_idx_auto, &automatic);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(automatic, 1);

			/* number of auto arenas should increase */
			ret = pmemobj_ctl_get(pop, "heap.narenas.automatic",
					&all_auto);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(narenas_b + i + 1, all_auto);
		}

		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &narenas_a);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(narenas_b + narenas_n, narenas_a);
	} else {
		UT_ASSERT(0);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
