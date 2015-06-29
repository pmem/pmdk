/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 * vmem_fork.c -- unit test for vmem_fork
 *
 * usage: vmem_fork 0|1 [directory]
 */

#include "unittest.h"

static int custom_allocs;
static int custom_alloc_calls;

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
 * create_pool -- create pool function
 *
 * This function calls vmem_create() for file if dir is not NULL,
 * or call vmem_create_in_region() with memory mapped from system.
 */
VMEM *
create_pool(const char *dir)
{
	VMEM *vmp;
	if (dir == NULL) {
		void *mem_pool;
		/* allocate memory for function vmem_create_in_region() */
		mem_pool = MMAP_ANON_ALIGNED(VMEM_MIN_POOL, 4 << 20);

		vmp = vmem_create_in_region(mem_pool, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_create_in_region");
	} else {
		vmp = vmem_create(dir, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_create");
	}
	return vmp;
}

int
main(int argc, char *argv[])
{
	const char *text_shared = "Text allocated from SHARED pool.";
	const char *text_parent = "Text allocated from PARENT pool.";
	const char *text_child = "Text allocated from CHILD pool.";
	int expect_custom_alloc = 0;
	char *dir = NULL;
	VMEM *vmp_shared; /* can be modified only by parent process */
	VMEM *vmp;
	int child_pid;
	char *str_shared;
	char *str;

	START(argc, argv, "vmem_fork");

	if (argc > 3 || argc < 2) {
		FATAL("usage: %s 0|1 [directory]", argv[0]);
	} else {
		expect_custom_alloc = atoi(argv[1]);
		if (argc > 2)
			dir = argv[3];
	}

	if (expect_custom_alloc)
		vmem_set_funcs(malloc_custom, free_custom,
			realloc_custom, strdup_custom, NULL);

	vmp_shared = create_pool(dir);
	str_shared = vmem_strdup(vmp_shared, text_shared);

	child_pid = fork();
	ASSERT(child_pid >= 0);

	if (child_pid == 0) {
		/* child */
		custom_allocs = 0;
		custom_alloc_calls = 0;
	}

	vmp = create_pool(dir);

	if (child_pid > 0) {
		/* parent */
		str = vmem_strdup(vmp, text_parent);
		ASSERTeq(strcmp(str, text_parent), 0);
	} else {
		/* child */
		str = vmem_strdup(vmp, text_child);
		ASSERTeq(strcmp(str, text_child), 0);
	}

	/* child process can only read memory allocated before fork */
	ASSERTeq(strcmp(str_shared, text_shared), 0);
	vmem_stats_print(vmp, NULL);

	vmem_free(vmp, str);
	ASSERTeq(vmem_check(vmp), 1);
	vmem_delete(vmp);

	if (child_pid > 0) {
		/* parent */
		waitpid(child_pid, NULL, 0);

		/* only parent can modify shared pool */
		vmem_stats_print(vmp_shared, NULL);
		vmem_free(vmp_shared, str_shared);
		ASSERTeq(vmem_check(vmp_shared), 1);
		vmem_delete(vmp_shared);
	}

	/* check memory leak in custom allocator */
	ASSERTeq(custom_allocs, 0);
	if (expect_custom_alloc == 0) {
		ASSERTeq(custom_alloc_calls, 0);
	} else {
		ASSERTne(custom_alloc_calls, 0);
	}

	if (child_pid > 0) {
		/* parent */
		OUT("Finish Parent");
		DONE(NULL);
	} else {
		/* child */
		OUT("Finish Child");
	}

}
