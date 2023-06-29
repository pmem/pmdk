// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * set_funcs.c -- unit test for pmem*_set_funcs()
 */
#include "unittest.h"

#define EXISTING_FILE "/root"
#define NON_ZERO_POOL_SIZE 1

#define GUARD 0x2BEE5AFEULL
#define EXTRA sizeof(GUARD)

#define OBJ 0

static struct counters {
	int mallocs;
	int frees;
	int reallocs;
	int reallocs_null;
	int strdups;
} cnt[5];

static void *
test_malloc(size_t size)
{
	unsigned long long *p = malloc(size + EXTRA);
	UT_ASSERTne(p, NULL);
	*p = GUARD;
	return ++p;
}

static void
test_free(void *ptr)
{
	if (ptr == NULL)
		return;
	unsigned long long *p = ptr;
	--p;
	UT_ASSERTeq(*p, GUARD);
	free(p);
}

static void *
test_realloc(void *ptr, size_t size)
{
	unsigned long long *p;
	if (ptr != NULL) {
		p = ptr;
		--p;
		UT_ASSERTeq(*p, GUARD);
		p = realloc(p, size + EXTRA);
	} else {
		p = malloc(size + EXTRA);
	}
	UT_ASSERTne(p, NULL);
	*p = GUARD;

	return ++p;
}

static char *
test_strdup(const char *s)
{
	if (s == NULL)
		return NULL;
	size_t size = strlen(s) + 1;
	unsigned long long *p = malloc(size + EXTRA);
	UT_ASSERTne(p, NULL);
	*p = GUARD;
	++p;
	strcpy((char *)p, s);
	return (char *)p;
}

static void *
obj_malloc(size_t size)
{
	cnt[OBJ].mallocs++;
	return test_malloc(size);
}

static void
obj_free(void *ptr)
{
	if (ptr)
		cnt[OBJ].frees++;
	test_free(ptr);
}

static void *
obj_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		cnt[OBJ].reallocs_null++;
	else
		cnt[OBJ].reallocs++;
	return test_realloc(ptr, size);
}

static char *
obj_strdup(const char *s)
{
	cnt[OBJ].strdups++;
	return test_strdup(s);
}

/*
 * There are a few allocations made at first call to pmemobj_open() or
 * pmemobj_create().  They are related to some global structures
 * holding a list of all open pools.  These allocation are not released on
 * pmemobj_close(), but in the library destructor.  So, we need to take them
 * into account when detecting memory leaks.
 *
 * obj_init/obj_pool_init:
 *   critnib_new  - Malloc + Zalloc
 *   ctree_new   - Malloc
 * lane_info_ht_boot/lane_info_create:
 *   critnib_new  - Malloc + Zalloc
 */
#define OBJ_EXTRA_NALLOC 6

static void
test_obj(const char *path)
{
	pmemobj_set_funcs(obj_malloc, obj_free, obj_realloc, obj_strdup);

	/*
	 * Generate ERR() call, that calls malloc() once,
	 * but only when it is called for the first time
	 * (free() is called in the destructor of the library).
	 */
	pmemobj_create(EXISTING_FILE, "", NON_ZERO_POOL_SIZE, 0);

	memset(cnt, 0, sizeof(cnt));

	PMEMobjpool *pop;
	pop = pmemobj_create(path, NULL, PMEMOBJ_MIN_POOL, 0600);

	PMEMoid oid;

	if (pmemobj_alloc(pop, &oid, 10, 0, NULL, NULL))
		UT_FATAL("!alloc");

	if (pmemobj_realloc(pop, &oid, 100, 0))
		UT_FATAL("!realloc");

	pmemobj_free(&oid);

	pmemobj_close(pop);

	UT_OUT("obj_mallocs: %d", cnt[OBJ].mallocs);
	UT_OUT("obj_frees: %d", cnt[OBJ].frees);
	UT_OUT("obj_reallocs: %d", cnt[OBJ].reallocs);
	UT_OUT("obj_reallocs_null: %d", cnt[OBJ].reallocs_null);
	UT_OUT("obj_strdups: %d", cnt[OBJ].strdups);

	if (cnt[OBJ].mallocs == 0 || cnt[OBJ].frees == 0)
		UT_FATAL("OBJ mallocs: %d, frees: %d", cnt[OBJ].mallocs,
				cnt[OBJ].frees);

	for (int i = 0; i < 5; ++i) {
		if (i == OBJ)
			continue;
		if (cnt[i].mallocs || cnt[i].frees)
			UT_FATAL("OBJ allocation used %d functions", i);
	}

	if (cnt[OBJ].mallocs + cnt[OBJ].strdups + cnt[OBJ].reallocs_null !=
					cnt[OBJ].frees + OBJ_EXTRA_NALLOC)
		UT_FATAL("OBJ memory leak");

	UNLINK(path);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "set_funcs");

	if (argc < 3)
		UT_FATAL("usage: %s file dir", argv[0]);

	test_obj(argv[1]);

	DONE(NULL);
}
