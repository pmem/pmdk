/*
 * Copyright 2015-2018, Intel Corporation
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

#define GUARD 0x2BEE5AFEULL
#define EXTRA sizeof(GUARD)

#define VMEM_ 0

#define VMEM_POOLS 4


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
	if (ptr == NULL)
		cnt[VMEM_].reallocs_null++;
	else
		cnt[VMEM_].reallocs++;
	return test_realloc(ptr, size);
}

static char *
_vmem_strdup(const char *s)
{
	cnt[VMEM_].strdups++;
	return test_strdup(s);
}

static void
test_vmem(const char *dir)
{
	vmem_set_funcs(_vmem_malloc, _vmem_free, _vmem_realloc, _vmem_strdup,
			NULL);

	/*
	 * Generate ERR() call, that calls malloc() once,
	 * but only when it is called for the first time
	 * (free() is called in the destructor of the library).
	 */
	vmem_create(EXISTING_FILE, 0);

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
	UT_OUT("vmem_reallocs_null: %d", cnt[VMEM_].reallocs_null);
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

	if (cnt[VMEM_].mallocs + cnt[VMEM_].strdups + cnt[VMEM_].reallocs_null
					> cnt[VMEM_].frees + 4)
		UT_FATAL("VMEM memory leak");
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "set_funcs");

	if (argc < 3)
		UT_FATAL("usage: %s file dir", argv[0]);

	test_vmem(argv[2]);

	DONE(NULL);
}
