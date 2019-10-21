/*
 * Copyright 2015-2019, Intel Corporation
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
 * set_funcs.c -- unit test for pmem*_set_funcs()
 */
#include "unittest.h"

#define EXISTING_FILE "/root"
#define NON_ZERO_POOL_SIZE 1

#define GUARD 0x2BEE5AFEULL
#define EXTRA sizeof(GUARD)

#define OBJ 0
#define BLK 1
#define LOG 2

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

static void *
blk_malloc(size_t size)
{
	cnt[BLK].mallocs++;
	return test_malloc(size);
}

static void
blk_free(void *ptr)
{
	if (ptr)
		cnt[BLK].frees++;
	test_free(ptr);
}

static void *
blk_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		cnt[BLK].reallocs_null++;
	else
		cnt[BLK].reallocs++;
	return test_realloc(ptr, size);
}

static char *
blk_strdup(const char *s)
{
	cnt[BLK].strdups++;
	return test_strdup(s);
}

static void *
log_malloc(size_t size)
{
	cnt[LOG].mallocs++;
	return test_malloc(size);
}

static void
log_free(void *ptr)
{
	if (ptr)
		cnt[LOG].frees++;
	test_free(ptr);
}

static void *
log_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		cnt[LOG].reallocs_null++;
	else
		cnt[LOG].reallocs++;
	return test_realloc(ptr, size);
}

static char *
log_strdup(const char *s)
{
	cnt[LOG].strdups++;
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

static void
test_blk(const char *path)
{
	pmemblk_set_funcs(blk_malloc, blk_free, blk_realloc, blk_strdup);

	/*
	 * Generate ERR() call, that calls malloc() once,
	 * but only when it is called for the first time
	 * (free() is called in the destructor of the library).
	 */
	pmemblk_create(EXISTING_FILE,  0, NON_ZERO_POOL_SIZE, 0);

	memset(cnt, 0, sizeof(cnt));

	PMEMblkpool *blk = pmemblk_create(path, 512, PMEMBLK_MIN_POOL, 0600);

	pmemblk_close(blk);

	UT_OUT("blk_mallocs: %d", cnt[BLK].mallocs);
	UT_OUT("blk_frees: %d", cnt[BLK].frees);
	UT_OUT("blk_reallocs: %d", cnt[BLK].reallocs);
	UT_OUT("blk_reallocs_null: %d", cnt[BLK].reallocs_null);
	UT_OUT("blk_strdups: %d", cnt[BLK].strdups);

	if (cnt[BLK].mallocs == 0 || cnt[BLK].frees == 0)
		UT_FATAL("BLK mallocs: %d, frees: %d", cnt[BLK].mallocs,
				cnt[BLK].frees);

	for (int i = 0; i < 5; ++i) {
		if (i == BLK)
			continue;
		if (cnt[i].mallocs || cnt[i].frees)
			UT_FATAL("BLK allocation used %d functions", i);
	}

	if (cnt[BLK].mallocs + cnt[BLK].strdups + cnt[BLK].reallocs_null
					!= cnt[BLK].frees)
		UT_FATAL("BLK memory leak");

	UNLINK(path);
}

static void
test_log(const char *path)
{
	pmemlog_set_funcs(log_malloc, log_free, log_realloc, log_strdup);

	/*
	 * Generate ERR() call, that calls malloc() once,
	 * but only when it is called for the first time
	 * (free() is called in the destructor of the library).
	 */
	pmemlog_create(EXISTING_FILE, NON_ZERO_POOL_SIZE, 0);

	memset(cnt, 0, sizeof(cnt));

	PMEMlogpool *log = pmemlog_create(path, PMEMLOG_MIN_POOL, 0600);

	pmemlog_close(log);

	UT_OUT("log_mallocs: %d", cnt[LOG].mallocs);
	UT_OUT("log_frees: %d", cnt[LOG].frees);
	UT_OUT("log_reallocs: %d", cnt[LOG].reallocs);
	UT_OUT("log_reallocs_null: %d", cnt[LOG].reallocs_null);
	UT_OUT("log_strdups: %d", cnt[LOG].strdups);

	if (cnt[LOG].mallocs == 0 || cnt[LOG].frees == 0)
		UT_FATAL("LOG mallocs: %d, frees: %d", cnt[LOG].mallocs,
				cnt[LOG].frees);

	for (int i = 0; i < 5; ++i) {
		if (i == LOG)
			continue;
		if (cnt[i].mallocs || cnt[i].frees)
			UT_FATAL("LOG allocation used %d functions", i);
	}

	if (cnt[LOG].mallocs + cnt[LOG].strdups + cnt[LOG].reallocs_null
					!= cnt[LOG].frees)
		UT_FATAL("LOG memory leak");

	UNLINK(path);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "set_funcs");

	if (argc < 3)
		UT_FATAL("usage: %s file dir", argv[0]);

	test_obj(argv[1]);
	test_blk(argv[1]);
	test_log(argv[1]);

	DONE(NULL);
}
