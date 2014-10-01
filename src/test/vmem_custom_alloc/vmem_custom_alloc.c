/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * vmem_custom_alloc.c -- unit test for vmem_custom_alloc
 *
 * usage: vmem_custom_alloc (0-2) [directory]
 */

#include "unittest.h"

#define	TEST_STRING_VALUE "Some test text, to check memory"
#define	TEST_REPEAT_CREATE_POOLS (20)

static int custom_allocs;
static int custom_alloc_calls;
static int expect_create_pool;

/*
 * malloc_null -- custom malloc function with error
 *
 * This function updates statistics about custom alloc functions,
 * and returns NULL.
 */
void *
malloc_null(size_t size)
{
	++custom_alloc_calls;
	return NULL;
}

/*
 * malloc_custom -- custom malloc function
 *
 * This function updates statistics about custom alloc functions,
 * and returns allocated memory.
 */
void *
malloc_custom(size_t size)
{
	++custom_alloc_calls;
	++custom_allocs;
	return malloc(size);
}

/*
 * free_custom -- custom free function
 *
 * This function updates statistics about custom alloc functions,
 * and frees allocated memory.
 */
void
free_custom(void *ptr)
{
	++custom_alloc_calls;
	--custom_allocs;
	free(ptr);
}

/*
 * realloc_custom -- custom realloc function
 *
 * This function updates statistics about custom alloc functions,
 * and returns reallocated memory.
 */
void *
realloc_custom(void *ptr, size_t size)
{
	++custom_alloc_calls;
	return realloc(ptr, size);
}

/*
 * strdup_custom -- custom strdup function
 *
 * This function updates statistics about custom alloc functions,
 * and returns allocated memory with a duplicated string.
 */
char *
strdup_custom(const char *s)
{
	++custom_alloc_calls;
	++custom_allocs;
	return strdup(s);
}

/*
 * pool_test -- test pool
 *
 * This function creates a memory pool in a file (if dir is not NULL),
 * or in RAM (if dir is NULL) and allocates memory for the test.
 */
void
pool_test(const char *dir)
{
	VMEM *vmp = NULL;

	if (dir != NULL) {
		vmp = vmem_pool_create(dir, VMEM_MIN_POOL);
	} else {
		/* allocate memory for function vmem_pool_create_in_region() */
		void *mem_pool = MMAP(NULL, VMEM_MIN_POOL, PROT_READ|PROT_WRITE,
					MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

		vmp = vmem_pool_create_in_region(mem_pool, VMEM_MIN_POOL);
	}

	if (expect_create_pool == 0) {
		ASSERTeq(vmp, NULL);
		DONE(NULL);
	} else {
		if (vmp == NULL) {
			if (dir == NULL) {
				FATAL("!vmem_pool_create_in_region");
			} else {
				FATAL("!vmem_pool_create");
			}
		}
	}

	char *test = vmem_malloc(vmp, strlen(TEST_STRING_VALUE) + 1);
	ASSERTne(test, NULL);

	strcpy(test, TEST_STRING_VALUE);
	ASSERTeq(strcmp(test, TEST_STRING_VALUE), 0);

	vmem_free(vmp, test);

	vmem_pool_delete(vmp);
}

int
main(int argc, char *argv[])
{
	int expect_custom_alloc = 0;

	START(argc, argv, "vmem_custom_alloc");

	if (argc < 2 || argc > 3 || strlen(argv[1]) != 1)
		FATAL("usage: %s (0-2) [directory]", argv[0]);

	switch (argv[1][0]) {
		case '0': {
			/* use default allocator */
			expect_custom_alloc = 0;
			expect_create_pool = 1;
			break;
		}
		case '1': {
			/* error in custom malloc function */
			expect_custom_alloc = 1;
			expect_create_pool = 0;
			vmem_set_funcs(malloc_null, free_custom,
				realloc_custom, strdup_custom, NULL);
			break;
		}
		case '2': {
			/* use custom alloc functions */
			expect_custom_alloc = 1;
			expect_create_pool = 1;
			vmem_set_funcs(malloc_custom, free_custom,
				realloc_custom, strdup_custom, NULL);
			break;
		}
		default: {
			FATAL("usage: %s (0-2) [directory]", argv[0]);
			break;
		}
	}

	if (argc == 3) {
		pool_test(argv[2]);
	} else {
		int i;
		/* repeat create pool */
		for (i = 0; i < TEST_REPEAT_CREATE_POOLS; ++i)
			pool_test(NULL);
	}

	/* check memory leak in custom allocator */
	ASSERTeq(custom_allocs, 0);

	if (expect_custom_alloc == 0) {
		ASSERTeq(custom_alloc_calls, 0);
	} else {
		ASSERTne(custom_alloc_calls, 0);
	}

	DONE(NULL);
}
