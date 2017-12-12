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
 * cto_basic.c -- unit test for close-to-open persistence
 *
 * usage: cto_basic filename
 */

#include "unittest.h"

#define NALLOCS 100

static int *ptrs[NALLOCS * 2];

#define POOL_SIZE (PMEMCTO_MIN_POOL * 2)

static const struct {
	size_t size;
	size_t spacing;
} Check_sizes[] = {
		{.size = 10, .spacing = 8},
		{.size = 100, .spacing = 16},
		{.size = 200, .spacing = 32},
		{.size = 500, .spacing = 64},
		{.size = 1000, .spacing = 128},
		{.size = 2000, .spacing = 256},
		{.size = 3000, .spacing = 512},
		{.size = 1 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 2 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 3 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 4 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 5 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 6 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 7 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 8 * 1024 * 1024, .spacing = 4 * 1024 * 1024},
		{.size = 9 * 1024 * 1024, .spacing = 4 * 1024 * 1024}
	};


static void
test_alloc(PMEMctopool *pcp, unsigned start, unsigned cnt)
{
	unsigned i;
	for (i = start; i < start + cnt; ++i) {
		ptrs[i] = pmemcto_malloc(pcp, 16 * sizeof(int));
		UT_ASSERTne(ptrs[i], NULL);
		*(ptrs[i]) = i;
	}
}

static void
test_check(PMEMctopool *pcp, unsigned start, unsigned cnt)
{
	unsigned i;
	for (i = start; i < start + cnt; ++i) {
		size_t usize = pmemcto_malloc_usable_size(pcp, ptrs[i]);
		UT_ASSERT(usize >= 16 * sizeof(int));
		UT_ASSERTeq(*(ptrs[i]), i);
	}
}

static void
test_free(PMEMctopool *pcp, unsigned start, unsigned cnt)
{
	unsigned i;
	for (i = start; i < start + cnt; ++i) {
		pmemcto_free(pcp, ptrs[i]);
	}
}

static void
do_malloc(PMEMctopool *pcp)
{
	size_t sum_alloc = 0;
	int i = 0;

	/* test with multiple size of allocations from 8MB to 2B */
	for (size_t size = 8 * 1024 * 1024; size > 2; ++i, size /= 2) {
		ptrs[i] = pmemcto_malloc(pcp, size);
		if (ptrs[i] == NULL)
			continue;

		*ptrs[i] = 0x1111;
		UT_ASSERTeq(*ptrs[i], 0x1111);

		sum_alloc += size;

		/* check that pointer came from mem_pool */
		UT_ASSERTrange(ptrs[i], pcp, POOL_SIZE);
	}

	/* allocate more than half of pool size */
	UT_ASSERT(sum_alloc * 2 > 8 * 1024 * 1024);

	while (i > 0)
		pmemcto_free(pcp, ptrs[--i]);
}

static void
do_calloc(PMEMctopool *pcp)
{
	for (int count = 1; count < 1024; count *= 2) {
		for (int i = 0; i < NALLOCS; i++) {
			ptrs[i] = pmemcto_calloc(pcp, count, sizeof(int));
			UT_ASSERTne(ptrs[i], NULL);

			/* check that pointer came from mem_pool */
			UT_ASSERTrange(ptrs[i], pcp, POOL_SIZE);

			/* pmemcto_calloc should return zeroed memory */
			for (int j = 0; j < count; j++) {
				UT_ASSERTeq(ptrs[i][j], 0);
				ptrs[i][j] = 0x2222;
				UT_ASSERTeq(ptrs[i][j], 0x2222);
			}
		}

		for (int i = 0; i < NALLOCS; i++)
			pmemcto_free(pcp, ptrs[i]);
	}
}

static void
do_realloc(PMEMctopool *pcp)
{
	int *test = pmemcto_realloc(pcp, NULL, sizeof(int));
	UT_ASSERTne(test, NULL);

	test[0] = 0x3333;
	UT_ASSERTeq(test[0], 0x3333);

	/* check that pointer came from mem_pool */
	UT_ASSERTrange(test, pcp, POOL_SIZE);

	test = pmemcto_realloc(pcp, test, sizeof(int) * 10);
	UT_ASSERTne(test, NULL);
	UT_ASSERTeq(test[0], 0x3333);
	test[1] = 0x3333;
	test[9] = 0x3333;

	/* check that pointer came from mem_pool */
	UT_ASSERTrange(test, pcp, POOL_SIZE);

	pmemcto_free(pcp, test);
}

static void
do_malloc_usable_size(PMEMctopool *pcp)
{
	UT_ASSERTeq(pmemcto_malloc_usable_size(pcp, NULL), 0);

	int i;
	for (i = 0; i < (sizeof(Check_sizes) / sizeof(Check_sizes[0])); ++i) {
		size_t size = Check_sizes[i].size;

		void *ptr = pmemcto_malloc(pcp, size);
		UT_ASSERTne(ptr, NULL);

		size_t usable_size = pmemcto_malloc_usable_size(pcp, ptr);
		UT_ASSERT(usable_size >= size);

		if (usable_size - size > Check_sizes[i].spacing) {
			UT_FATAL("Size %zu: spacing %zu is bigger"
				"than expected: %zu", size,
				(usable_size - size), Check_sizes[i].spacing);
		}

		memset(ptr, 0xEE, usable_size);
		pmemcto_free(pcp, ptr);
	}
}

static void
do_strdup(PMEMctopool *pcp)
{
	const char *text = "Some test text";
	const char *text_empty = "";
	const wchar_t *wtext = L"Some test text";
	const wchar_t *wtext_empty = L"";

	char *str1 = pmemcto_strdup(pcp, text);
	wchar_t *wcs1 = pmemcto_wcsdup(pcp, wtext);
	UT_ASSERTne(str1, NULL);
	UT_ASSERTne(wcs1, NULL);
	UT_ASSERTeq(strcmp(text, str1), 0);
	UT_ASSERTeq(wcscmp(wtext, wcs1), 0);

	/* check that pointer came from mem_pool */
	UT_ASSERTrange(str1, pcp, POOL_SIZE);
	UT_ASSERTrange(wcs1, pcp, POOL_SIZE);

	char *str2 = pmemcto_strdup(pcp, text_empty);
	wchar_t *wcs2 = pmemcto_wcsdup(pcp, wtext_empty);
	UT_ASSERTne(str2, NULL);
	UT_ASSERTne(wcs2, NULL);
	UT_ASSERTeq(strcmp(text_empty, str2), 0);
	UT_ASSERTeq(wcscmp(wtext_empty, wcs2), 0);

	/* check that pointer came from mem_pool */
	UT_ASSERTrange(str2, pcp, POOL_SIZE);
	UT_ASSERTrange(wcs2, pcp, POOL_SIZE);

	pmemcto_free(pcp, str1);
	pmemcto_free(pcp, wcs1);
	pmemcto_free(pcp, str2);
	pmemcto_free(pcp, wcs2);
}

int
main(int argc, char *argv[])
{
	int *root;

	START(argc, argv, "cto_basic");

	if (argc != 2)
		UT_FATAL("usage: %s filename", argv[0]);

	UT_OUT("create: %s", argv[1]);
	PMEMctopool *pcp = pmemcto_create(argv[1], "cto_basic",
			0, S_IWUSR|S_IRUSR);
	if (pcp == NULL)
		UT_FATAL("!pmemcto_create");

	test_alloc(pcp, 0, NALLOCS);
	test_check(pcp, 0, NALLOCS);
	pmemcto_set_root_pointer(pcp, ptrs[0]);
	pmemcto_close(pcp);

	PMEMctopool *pcp_old = pcp;

	UT_OUT("re-open #1: %s", argv[1]);
	/* reopen */
	pcp = pmemcto_open(argv[1], "cto_basic");
	if (pcp == NULL)
		UT_FATAL("!pmemcto_open");
	UT_ASSERTeq(pcp, pcp_old);

	test_check(pcp, 0, NALLOCS);
	root = pmemcto_get_root_pointer(pcp);
	UT_ASSERTeq(root, ptrs[0]);
	test_alloc(pcp, NALLOCS, NALLOCS);
	test_check(pcp, NALLOCS, NALLOCS);
	pmemcto_set_root_pointer(pcp, ptrs[NALLOCS]);
	pmemcto_close(pcp);

	UT_OUT("re-open #2: %s", argv[1]);
	/* reopen */
	pcp = pmemcto_open(argv[1], "cto_basic");
	if (pcp == NULL)
		UT_FATAL("!pmemcto_open");
	UT_ASSERTeq(pcp, pcp_old);

	test_check(pcp, 0, 2 * NALLOCS);
	root = pmemcto_get_root_pointer(pcp);
	UT_ASSERTeq(root, ptrs[NALLOCS]);
	test_free(pcp, 0, 2 * NALLOCS);
	pmemcto_close(pcp);

	UT_OUT("re-open #3: %s", argv[1]);
	/* reopen */
	pcp = pmemcto_open(argv[1], "cto_basic");
	if (pcp == NULL)
		UT_FATAL("!pmemcto_open");
	UT_ASSERTeq(pcp, pcp_old);

	do_malloc(pcp);
	do_calloc(pcp);
	do_realloc(pcp);
	do_malloc_usable_size(pcp);
	do_strdup(pcp);

	pmemcto_close(pcp);

	/* try to open the pool when the base address is busy */
	void *ptr = mmap(pcp_old, 4096, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0);
	UT_ASSERTne(ptr, NULL);
	UT_ASSERTeq(ptr, pcp_old);
	pcp = pmemcto_open(argv[1], "cto_basic");
	UT_ASSERTeq(pcp, NULL);
	munmap(ptr, 4096);

	int ret = pmemcto_check(argv[1], "cto_basic");
	UT_ASSERTeq(ret, 1);

	ret = pmemcto_check(argv[1], NULL);
	UT_ASSERTeq(ret, 1);

	ret = pmemcto_check(argv[1], "xxx");
	UT_ASSERTeq(ret, -1);

	DONE(NULL);
}
