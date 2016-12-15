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
 * set_funcs.c -- unit test for pmem*_set_funcs()
 */
#include "unittest.h"

#define GUARD 0x2BEE5AFEULL
#define EXTRA sizeof(GUARD)

#define OBJ 0
#define BLK 1
#define LOG 2
#define CTO 3
#define VMEM_ 4

#define VMEM_POOLS 4


static struct counters {
	int mallocs;
	int frees;
	int reallocs;
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
	cnt[LOG].reallocs++;
	return test_realloc(ptr, size);
}

static char *
log_strdup(const char *s)
{
	cnt[LOG].strdups++;
	return test_strdup(s);
}

static void *
_vmem_malloc(size_t size)
{
	cnt[VMEM_].mallocs++;
	return test_malloc(size);
}

static void
_vmem_free(void *ptr)
{
	if (ptr)
		cnt[VMEM_].frees++;
	test_free(ptr);
}

static void *
_vmem_realloc(void *ptr, size_t size)
{
	cnt[VMEM_].reallocs++;
	return test_realloc(ptr, size);
}

static char *
_vmem_strdup(const char *s)
{
	cnt[VMEM_].strdups++;
	return test_strdup(s);
}

static void *
cto_malloc(size_t size)
{
	cnt[CTO].mallocs++;
	return test_malloc(size);
}

static void
cto_free(void *ptr)
{
	if (ptr)
		cnt[CTO].frees++;
	test_free(ptr);
}

static void *
cto_realloc(void *ptr, size_t size)
{
	cnt[CTO].reallocs++;
	return test_realloc(ptr, size);
}

static char *
cto_strdup(const char *s)
{
	cnt[CTO].strdups++;
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
 *   cuckoo_new  - Malloc + Zalloc
 *   ctree_new   - Malloc
 * lane_info_ht_boot/lane_info_create:
 *   cuckoo_new  - Malloc + Zalloc
 */
#define OBJ_EXTRA_NALLOC 5

static void
test_obj(const char *path)
{
	memset(cnt, 0, sizeof(cnt));

	PMEMobjpool *pop =
			pmemobj_create(path, NULL, PMEMOBJ_MIN_POOL, 0600);

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
	if (cnt[OBJ].mallocs + cnt[OBJ].strdups !=
					cnt[OBJ].frees + OBJ_EXTRA_NALLOC)
		UT_FATAL("OBJ memory leak");

	UNLINK(path);
}

static void
test_blk(const char *path)
{
	memset(cnt, 0, sizeof(cnt));

	PMEMblkpool *blk = pmemblk_create(path, 512, PMEMBLK_MIN_POOL, 0600);

	pmemblk_close(blk);


	UT_OUT("blk_mallocs: %d", cnt[BLK].mallocs);
	UT_OUT("blk_frees: %d", cnt[BLK].frees);
	UT_OUT("blk_reallocs: %d", cnt[BLK].reallocs);
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
	if (cnt[BLK].mallocs + cnt[BLK].strdups != cnt[BLK].frees)
		UT_FATAL("BLK memory leak");

	UNLINK(path);
}

static void
test_log(const char *path)
{
	memset(cnt, 0, sizeof(cnt));

	PMEMlogpool *log = pmemlog_create(path, PMEMLOG_MIN_POOL, 0600);

	pmemlog_close(log);

	UT_OUT("log_mallocs: %d", cnt[LOG].mallocs);
	UT_OUT("log_frees: %d", cnt[LOG].frees);
	UT_OUT("log_reallocs: %d", cnt[LOG].reallocs);
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
	if (cnt[LOG].mallocs + cnt[LOG].strdups != cnt[LOG].frees)
		UT_FATAL("LOG memory leak");

	UNLINK(path);
}

/*
 * There are a few allocations made at first call to pmemcto_malloc(),
 * pmemcto_realloc(), etc.
 * They are related to some global jemalloc structures in TSD, holding
 * a list of all open pools.  These allocation are not released on
 * pmemcto_close(), but in the library destructor.  So, we need to take them
 * into account when detecting memory leaks.
 * Same applies to errormsg buffer, which is allocated on the first error
 * and released in library dtor.
 *
 *   tcache_tsd  -     2 * Zalloc
 *   areanas_tsd     - 2 * Zalloc
 *   errormsg buffer - 1 * Malloc
 */
#define CTO_EXTRA_NALLOC 5

static void
test_cto(const char *path)
{
	memset(cnt, 0, sizeof(cnt));

	PMEMctopool *pcp =
		pmemcto_create(path, "test", PMEMCTO_MIN_POOL, 0600);

	/* 2*malloc + 1*strdup - pool set structure allocation (open/create) */
	UT_ASSERTeq(cnt[CTO].mallocs, 2);
	UT_ASSERTeq(cnt[CTO].reallocs, 0);
	UT_ASSERTeq(cnt[CTO].strdups, 1);
	UT_ASSERTeq(cnt[CTO].frees, 0);

	int ret = pmemcto_check(path, "xxx");
	UT_ASSERTeq(ret, -1);

	/* 1*malloc - errormsg buffer (first error) */
	UT_ASSERTeq(cnt[CTO].mallocs, 3);
	UT_ASSERTeq(cnt[CTO].reallocs, 0);
	UT_ASSERTeq(cnt[CTO].strdups, 1);
	UT_ASSERTeq(cnt[CTO].frees, 0);

	void *ptr = pmemcto_malloc(pcp, 10);
	UT_ASSERTne(ptr, NULL);

	/* 4*malloc - tcache_tsd + arenas_tsd (first pmemcto_malloc) */
	UT_ASSERTeq(cnt[CTO].mallocs, 7);
	UT_ASSERTeq(cnt[CTO].reallocs, 0);
	UT_ASSERTeq(cnt[CTO].strdups, 1);
	UT_ASSERTeq(cnt[CTO].frees, 0);

	ptr = pmemcto_realloc(pcp, ptr, 100);
	UT_ASSERTne(ptr, NULL);

	pmemcto_free(pcp, ptr);

	/* no change */
	UT_ASSERTeq(cnt[CTO].mallocs, 7);
	UT_ASSERTeq(cnt[CTO].reallocs, 0);
	UT_ASSERTeq(cnt[CTO].strdups, 1);
	UT_ASSERTeq(cnt[CTO].frees, 0);

	pmemcto_close(pcp);

	/* 3*free - pool set structure */
	UT_ASSERTeq(cnt[CTO].mallocs, 7);
	UT_ASSERTeq(cnt[CTO].reallocs, 0);
	UT_ASSERTeq(cnt[CTO].strdups, 1);
	UT_ASSERTeq(cnt[CTO].frees, 3);

	UT_OUT("cto_mallocs: %d", cnt[CTO].mallocs);
	UT_OUT("cto_frees: %d", cnt[CTO].frees);
	UT_OUT("cto_reallocs: %d", cnt[CTO].reallocs);
	UT_OUT("cto_strdups: %d", cnt[CTO].strdups);

	if (cnt[CTO].mallocs == 0 || cnt[CTO].frees == 0)
		UT_FATAL("CTO mallocs: %d, frees: %d", cnt[CTO].mallocs,
				cnt[CTO].frees);
	for (int i = 0; i < 5; ++i) {
		if (i == CTO)
			continue;
		if (cnt[i].mallocs || cnt[i].frees)
			UT_FATAL("CTO allocation used %d functions", i);
	}
	if (cnt[CTO].mallocs + cnt[CTO].strdups !=
					cnt[CTO].frees + CTO_EXTRA_NALLOC)
		UT_FATAL("CTO memory leak");

	UNLINK(path);
}

static void
test_vmem(const char *dir)
{
	memset(cnt, 0, sizeof(cnt));

	VMEM *v[VMEM_POOLS];
	void *ptr[VMEM_POOLS];

	for (int i = 0; i < VMEM_POOLS; i++) {
		v[i] = vmem_create(dir, VMEM_MIN_POOL);
		ptr[i] = vmem_malloc(v[i], 64);
		vmem_free(v[i], ptr[i]);
	}

	for (int i = 0; i < VMEM_POOLS; i++)
		vmem_delete(v[i]);

	UT_OUT("vmem_mallocs: %d", cnt[VMEM_].mallocs);
	UT_OUT("vmem_frees: %d", cnt[VMEM_].frees);
	UT_OUT("vmem_reallocs: %d", cnt[VMEM_].reallocs);
	UT_OUT("vmem_strdups: %d", cnt[VMEM_].strdups);

	if (cnt[VMEM_].mallocs == 0 && cnt[VMEM_].frees == 0)
		UT_FATAL("VMEM mallocs: %d, frees: %d", cnt[VMEM_].mallocs,
				cnt[VMEM_].frees);
	for (int i = 0; i < 5; ++i) {
		if (i == VMEM_)
			continue;
		if (cnt[i].mallocs || cnt[i].frees)
			UT_FATAL("VMEM allocation used %d functions", i);
	}
	if (cnt[VMEM_].mallocs + cnt[VMEM_].strdups > cnt[VMEM_].frees + 4)
		UT_FATAL("VMEM memory leak");
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "set_funcs");

	if (argc < 3)
		UT_FATAL("usage: %s file dir", argv[0]);

	pmemobj_set_funcs(obj_malloc, obj_free, obj_realloc, obj_strdup);
	pmemblk_set_funcs(blk_malloc, blk_free, blk_realloc, blk_strdup);
	pmemlog_set_funcs(log_malloc, log_free, log_realloc, log_strdup);
	pmemcto_set_funcs(cto_malloc, cto_free, cto_realloc, cto_strdup, NULL);
	vmem_set_funcs(_vmem_malloc, _vmem_free, _vmem_realloc, _vmem_strdup,
			NULL);

	test_obj(argv[1]);
	test_blk(argv[1]);
	test_log(argv[1]);
	test_cto(argv[1]);
	test_vmem(argv[2]);

	DONE(NULL);
}
