// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2024, Intel Corporation */

/*
 * obj_ctl_arenas.c -- tests for the ctl entry points
 * usage:
 * obj_ctl_arenas <file> n - test for heap.narenas.total
 *
 * obj_ctl_arenas <file> s - test for heap.arena.[idx].size
 * and heap.thread.arena_id (RW)
 *
 * obj_ctl_arenas <file> c - test for heap.arena.create,
 * heap.arena.[idx].automatic and heap.narenas.automatic
 * obj_ctl_arenas <file> a - mt test for heap.arena.create
 * and heap.thread.arena_id
 *
 * obj_ctl_arenas <file> f - test for POBJ_ARENA_ID flag,
 *
 * obj_ctl_arenas <file> g - test for POBJ_ARENA_ID with
 * non-exists arena id
 *
 * obj_ctl_arenas <file> q - test for programmatic change of
 *	heap.arenas_assignment_type (RW)
 *
 * obj_ctl_arenas <file> p - test for config change of
 *	heap.arenas_assignment_type for global type (RW)
 *
 * obj_ctl_arenas <file> d - test for config change of
 *	heap.arenas_assignment_type for thread key type (RW)
 *
 * obj_ctl_arenas <file> b - test for config change of
 *	heap.arenas_default_max
 */

#include <sched.h>
#include "libpmemobj/atomic_base.h"
#include "libpmemobj/ctl.h"
#include "os_thread.h"
#include "sys_util.h"
#include "unittest.h"
#include "util.h"

#define CHUNKSIZE ((size_t)1024 * 256)	/* 256 kilobytes */
#define LAYOUT "obj_ctl_arenas"
#define CTL_QUERY_LEN 256
#define NTHREAD 2
#define NTHREAD_ARENA 32
#define NOBJECT_THREAD 64
#define ALLOC_CLASS_ARENA 2
#define NTHREADX 16
#define NARENAS 16
#define DEFAULT_ARENAS_MAX (1 << 10)

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
	{
		.header_type = POBJ_HEADER_NONE,
		.unit_size = 111,
		.units_per_block = CHUNKSIZE / 111,
		.alignment = 0
	},
};

struct arena_alloc {
	unsigned arena;
	PMEMoid oid;
};

static struct arena_alloc ref;

static void
check_arena_size(unsigned arena_id, unsigned class_id)
{
	int ret;
	size_t arena_size;
	char arena_idx_size[CTL_QUERY_LEN];

	SNPRINTF(arena_idx_size, CTL_QUERY_LEN,
			"heap.arena.%u.size", arena_id);

	ret = pmemobj_ctl_get(pop, arena_idx_size, &arena_size);
	UT_ASSERTeq(ret, 0);

	size_t test = ALIGN_UP(alloc_class[class_id].unit_size *
			alloc_class[class_id].units_per_block, CHUNKSIZE);
	UT_ASSERTeq(test, arena_size);
}

static void
create_alloc_class(void)
{
	int ret;
	ret = pmemobj_ctl_set(pop, "heap.alloc_class.128.desc",
			&alloc_class[0]);
	UT_ASSERTeq(ret, 0);
	ret = pmemobj_ctl_set(pop, "heap.alloc_class.129.desc",
			&alloc_class[1]);
	UT_ASSERTeq(ret, 0);
}

static void *
worker_arenas_size(void *arg)
{
	int ret = -1;
	int idx = (int)(intptr_t)arg;
	int off_idx = idx + 128;
	unsigned arena_id;
	unsigned arena_id_new;

	ret = pmemobj_ctl_exec(pop, "heap.arena.create",
			&arena_id_new);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(arena_id_new >= 1);

	ret = pmemobj_ctl_set(pop, "heap.thread.arena_id",
			&arena_id_new);
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

	check_arena_size(arena_id, (unsigned)idx);

	return NULL;
}

static void *
worker_arenas_flag(void *arg)
{
	int ret;
	unsigned arenas[NARENAS];
	for (unsigned i = 0; i < NARENAS; ++i) {
		ret = pmemobj_ctl_exec(pop, "heap.arena.create",
				&arenas[i]);
		UT_ASSERTeq(ret, 0);
	}

	/*
	 * Tests POBJ_ARENA_ID with pmemobj_xalloc.
	 * All object are frees after pthread join.
	 */
	for (unsigned i = 0; i < 2; i++) {
		ret = pmemobj_xalloc(pop,
				NULL, alloc_class[i].unit_size, 0,
				POBJ_CLASS_ID(i + 128) | \
				POBJ_ARENA_ID(arenas[i]),
				NULL, NULL);
		UT_ASSERTeq(ret, 0);
		check_arena_size(arenas[i], i);
	}

	/* test POBJ_ARENA_ID with pmemobj_xreserve */
	struct pobj_action act;
	PMEMoid oid = pmemobj_xreserve(pop, &act,
			alloc_class[0].unit_size, 1,
			POBJ_CLASS_ID(128) |
			POBJ_ARENA_ID(arenas[2]));
	pmemobj_publish(pop, &act, 1);
	pmemobj_free(&oid);
	UT_ASSERT(OID_IS_NULL(oid));

	/* test POBJ_ARENA_ID with pmemobj_tx_xalloc */
	TX_BEGIN(pop) {
		pmemobj_tx_xalloc(alloc_class[1].unit_size, 0,
				POBJ_CLASS_ID(129) | POBJ_ARENA_ID(arenas[3]));
	} TX_END
	check_arena_size(arenas[3], 1);

	return NULL;
}

static void *
worker_arena_threads(void *arg)
{
	int ret = -1;
	struct arena_alloc *ref = (struct arena_alloc *)arg;
	unsigned arena_id;

	ret = pmemobj_ctl_get(pop, "heap.thread.arena_id", &arena_id);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(arena_id != 0);

	ret = pmemobj_ctl_set(pop, "heap.thread.arena_id", &ref->arena);
	UT_ASSERTeq(ret, 0);

	PMEMoid oid[NOBJECT_THREAD];

	for (int i = 0; i < NOBJECT_THREAD; i++) {
		ret = pmemobj_xalloc(pop, &oid[i],
				alloc_class[ALLOC_CLASS_ARENA].unit_size,
				0, POBJ_CLASS_ID(ALLOC_CLASS_ARENA + 128),
				NULL, NULL);
		UT_ASSERTeq(ret, 0);

		/* objects are in the same block as the first one */
		ASSERT(labs((long)ref->oid.off - (long)oid[i].off) <=
			alloc_class[ALLOC_CLASS_ARENA].unit_size *
			(alloc_class[ALLOC_CLASS_ARENA].units_per_block - 1));
	}

	for (int i = 0; i < NOBJECT_THREAD; i++)
		pmemobj_free(&oid[i]);

	return NULL;
}

static void
worker_arena_ref_obj(struct arena_alloc *ref)
{
	int ret = -1;

	ret = pmemobj_ctl_set(pop, "heap.thread.arena_id", &ref->arena);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_xalloc(pop, &ref->oid,
			alloc_class[ALLOC_CLASS_ARENA].unit_size,
			0, POBJ_CLASS_ID(ALLOC_CLASS_ARENA + 128), NULL, NULL);
	UT_ASSERTeq(ret, 0);
}

#define MAX_KEYS 1024

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_arenas");

	if (argc != 3)
		UT_FATAL("usage: %s poolset [n|s|c|f|q|m|a|g|p|d|b]", argv[0]);

	const char *path = argv[1];
	char t = argv[2][0];

	int ret = 0;

	int keys_created = 0;
	size_t target_narenas = 0;
	if (t == 'g') {
		os_tls_key_t *keys = MALLOC(sizeof(os_tls_key_t) * MAX_KEYS);
		int i;
		for (i = 0; i < MAX_KEYS; ++i) {
			if (os_tls_key_create(&keys[i], NULL) != 0)
				break;
		}
		keys_created = i;

		for (i = 0; i < keys_created; ++i) {
			os_tls_key_delete(keys[i]);
		}

		FREE(keys);

		enum pobj_arenas_assignment_type atype;

		ret = pmemobj_ctl_get(pop,
			"heap.arenas_assignment_type", &atype);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(atype, POBJ_ARENAS_ASSIGNMENT_THREAD_KEY);

		atype = POBJ_ARENAS_ASSIGNMENT_GLOBAL;

		ret = pmemobj_ctl_set(NULL,
			"heap.arenas_assignment_type", &atype);
		UT_ASSERTeq(ret, 0);
	} else if (t == 'b') {
		size_t narenas = 0;
		ret = pmemobj_ctl_get(pop,
			"heap.arenas_default_max", &narenas);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTne(narenas, 0);

		target_narenas = narenas + 1;
		ret = pmemobj_ctl_set(pop,
			"heap.arenas_default_max", &target_narenas);
		UT_ASSERTeq(ret, 0);
	}

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL * 20,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	if (t == 'n') {
		unsigned narenas = 0;
		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &narenas);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTne(narenas, 0);
	} else if (t == 's') {
		os_thread_t threads[NTHREAD];
		util_mutex_init(&lock);
		util_cond_init(&cond);

		create_alloc_class();
		for (int i = 0; i < NTHREAD; i++)
			THREAD_CREATE(&threads[i], NULL, worker_arenas_size,
					(void *)(intptr_t)i);

		for (int i = 0; i < NTHREAD; i++)
			THREAD_JOIN(&threads[i], NULL);

		PMEMoid oid, oid2;
		POBJ_FOREACH_SAFE(pop, oid, oid2)
			pmemobj_free(&oid);

		util_mutex_destroy(&lock);
		util_cond_destroy(&cond);
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
		for (unsigned i = 1; i <= narenas_b; i++) {
			SNPRINTF(arena_idx_auto, CTL_QUERY_LEN,
					"heap.arena.%u.automatic", i);

			ret = pmemobj_ctl_get(pop, arena_idx_auto, &automatic);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(automatic, 1);
		}
		ret = pmemobj_ctl_get(pop, "heap.narenas.automatic", &all_auto);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(narenas_b, all_auto);

		/* all arenas created by user should not be auto  */
		for (unsigned i = 1; i <= narenas_n; i++) {
			ret = pmemobj_ctl_exec(pop, "heap.arena.create",
					&arena_id);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(arena_id, narenas_b + i);

			SNPRINTF(arena_idx_auto, CTL_QUERY_LEN,
					"heap.arena.%u.automatic", arena_id);

			ret = pmemobj_ctl_get(pop, arena_idx_auto, &automatic);
			UT_ASSERTeq(automatic, 0);

			/*
			 * after creation, number of auto
			 * arenas should be the same
			 */
			ret = pmemobj_ctl_get(pop, "heap.narenas.automatic",
					&all_auto);
			UT_ASSERTeq(ret, 0);
			UT_ASSERTeq(narenas_b + i - 1, all_auto);

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
			UT_ASSERTeq(narenas_b + i, all_auto);
		}

		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &narenas_a);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(narenas_b + narenas_n, narenas_a);

		/* at least one automatic arena must exist */
		for (unsigned i = 1; i <= narenas_a; i++) {
			SNPRINTF(arena_idx_auto, CTL_QUERY_LEN,
					"heap.arena.%u.automatic", i);

			automatic = 0;
			if (i < narenas_a) {
				ret = pmemobj_ctl_set(pop, arena_idx_auto,
						&automatic);
				UT_ASSERTeq(ret, 0);
			} else {
				/*
				 * last auto arena -
				 * cannot change the state to 0...
				 */
				ret = pmemobj_ctl_set(pop, arena_idx_auto,
						&automatic);
				UT_ASSERTeq(ret, -1);

				/* ...but can change (overwrite) to 1 */
				automatic = 1;
				ret = pmemobj_ctl_set(pop, arena_idx_auto,
						&automatic);
				UT_ASSERTeq(ret, 0);
			}
		}
	} else if (t == 'a') {
		int ret;
		unsigned arena_id_new;
		char alloc_class_idx_desc[CTL_QUERY_LEN];

		ret = pmemobj_ctl_exec(pop, "heap.arena.create",
				&arena_id_new);
		UT_ASSERTeq(ret, 0);
		UT_ASSERT(arena_id_new >= 1);

		SNPRINTF(alloc_class_idx_desc, CTL_QUERY_LEN,
				"heap.alloc_class.%d.desc",
				ALLOC_CLASS_ARENA + 128);

		ret = pmemobj_ctl_set(pop, alloc_class_idx_desc,
				&alloc_class[ALLOC_CLASS_ARENA]);
		UT_ASSERTeq(ret, 0);

		ref.arena = arena_id_new;
		worker_arena_ref_obj(&ref);

		os_thread_t threads[NTHREAD_ARENA];

		for (int i = 0; i < NTHREAD_ARENA; i++) {
			THREAD_CREATE(&threads[i], NULL, worker_arena_threads,
					&ref);
		}

		for (int i = 0; i < NTHREAD_ARENA; i++)
			THREAD_JOIN(&threads[i], NULL);
	} else if (t == 'f') {
		os_thread_t threads[NTHREADX];

		create_alloc_class();

		for (int i = 0; i < NTHREADX; i++)
			THREAD_CREATE(&threads[i], NULL,
					worker_arenas_flag, NULL);

		for (int i = 0; i < NTHREADX; i++)
			THREAD_JOIN(&threads[i], NULL);

		PMEMoid oid, oid2;
		POBJ_FOREACH_SAFE(pop, oid, oid2)
			pmemobj_free(&oid);
	} else if (t == 'q') {
		unsigned total;
		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &total);
		UT_ASSERTeq(ret, 0);

		ret = pmemobj_xalloc(pop, NULL, alloc_class[0].unit_size, 0,
				POBJ_ARENA_ID(total), NULL, NULL);
		UT_ASSERTne(ret, 0);
	} else if (t == 'm') {
		unsigned max;
		unsigned new_max;

		ret = pmemobj_ctl_get(pop, "heap.narenas.max", &max);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(DEFAULT_ARENAS_MAX, max);

		/* size should not decrease */
		new_max = DEFAULT_ARENAS_MAX - 1;
		ret = pmemobj_ctl_set(pop, "heap.narenas.max", &new_max);
		UT_ASSERTne(ret, 0);
		ret = pmemobj_ctl_get(pop, "heap.narenas.max", &max);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(DEFAULT_ARENAS_MAX, max);

		/* size should increase */
		new_max = DEFAULT_ARENAS_MAX + 1;
		ret = pmemobj_ctl_set(pop, "heap.narenas.max", &new_max);
		UT_ASSERTeq(ret, 0);
		ret = pmemobj_ctl_get(pop, "heap.narenas.max", &max);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(DEFAULT_ARENAS_MAX + 1, max);
	} else if (t == 'g') {
		enum pobj_arenas_assignment_type atype;
		ret = pmemobj_ctl_get(pop,
			"heap.arenas_assignment_type", &atype);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(atype, POBJ_ARENAS_ASSIGNMENT_GLOBAL);

		/* verify that we can actually allocate something */
		ret = pmemobj_alloc(pop, NULL, 1, 0, NULL, NULL);
		UT_ASSERTeq(ret, 0);

		/* tests if any tls keys were used up during pool creation */
		os_tls_key_t *keys = MALLOC(sizeof(os_tls_key_t) * MAX_KEYS);
		int i;
		for (i = 0; i < MAX_KEYS; ++i) {
			if (os_tls_key_create(&keys[i], NULL) != 0)
				break;
		}
		int keys_created_second = i;

		UT_ASSERTeq(keys_created, keys_created_second);

		for (i = 0; i < keys_created_second; ++i) {
			os_tls_key_delete(keys[i]);
		}

		FREE(keys);
	} else if (t == 'p') {
		enum pobj_arenas_assignment_type atype;
		ret = pmemobj_ctl_get(pop,
			"heap.arenas_assignment_type", &atype);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(atype, POBJ_ARENAS_ASSIGNMENT_GLOBAL);
	} else if (t == 'd') {
		enum pobj_arenas_assignment_type atype;
		ret = pmemobj_ctl_get(pop,
			"heap.arenas_assignment_type", &atype);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(atype, POBJ_ARENAS_ASSIGNMENT_THREAD_KEY);
	} else if (t == 'b') {
		unsigned narenas = 0;
		ret = pmemobj_ctl_get(pop, "heap.narenas.total", &narenas);
		UT_ASSERTeq(ret, 0);
		UT_ASSERTeq(narenas, target_narenas);
	} else {
		UT_ASSERT(0);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
