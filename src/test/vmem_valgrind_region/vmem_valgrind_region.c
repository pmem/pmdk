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
 * vmem_valgrind_region.c -- unit test for vmem_valgrind_region
 */

#include "unittest.h"

#define POOLSIZE (16 << 20)
#define CHUNKSIZE (4 << 20)
#define NOBJS 8

struct foo {
	size_t size;
	char data[1]; /* dynamically sized */
};

static struct foo *objs[NOBJS];

static void
do_alloc(VMEM *vmp)
{
	size_t size = 256;
	/* allocate objects */
	for (int i = 0; i < NOBJS; i++) {
		objs[i] = vmem_malloc(vmp, size + sizeof(size_t));
		UT_ASSERTne(objs[i], NULL);
		objs[i]->size = size;
		memset(objs[i]->data, '0' + i, size - 1);
		objs[i]->data[size] = '\0';
		size *= 4;
	}
}

static void
do_iterate(void)
{
	/* dump selected objects */
	for (int i = 0; i < NOBJS; i++)
		UT_OUT("%p size %zu", objs[i], objs[i]->size);
}

static void
do_free(VMEM *vmp)
{
	/* free objects */
	for (int i = 0; i < NOBJS; i++)
		vmem_free(vmp, objs[i]);
}

int
main(int argc, char *argv[])
{
	VMEM *vmp;

	START(argc, argv, "vmem_valgrind_region");

	if (argc < 2)
		UT_FATAL("usage: %s <0..4>", argv[0]);

	int test = atoi(argv[1]);

	/*
	 * Allocate memory for vmem_create_in_region().
	 * Reserve more space for test case #4.
	 */
	char *addr = MMAP_ANON_ALIGNED(VMEM_MIN_POOL + CHUNKSIZE,
			CHUNKSIZE);

	vmp = vmem_create_in_region(addr, POOLSIZE);
	if (vmp == NULL)
		UT_FATAL("!vmem_create_in_region");

	do_alloc(vmp);

	switch (test) {
	case 0:
		/* free objects and delete pool */
		do_free(vmp);
		vmem_delete(vmp);
		break;

	case 1:
		/* delete pool without freeing objects */
		vmem_delete(vmp);
		break;

	case 2:
		/*
		 * delete pool without freeing objects
		 * try to access objects
		 * expected: use of uninitialized value
		 */
		vmem_delete(vmp);
		do_iterate();
		break;

	case 3:
		/*
		 * delete pool without freeing objects
		 * re-create pool in the same region
		 * try to access objects
		 * expected: invalid read
		 */
		vmem_delete(vmp);
		vmp = vmem_create_in_region(addr, POOLSIZE);
		if (vmp == NULL)
			UT_FATAL("!vmem_create_in_region");
		do_iterate();
		vmem_delete(vmp);
		break;

	case 4:
		/*
		 * delete pool without freeing objects
		 * re-create pool in the overlapping region
		 * try to access objects
		 * expected: use of uninitialized value & invalid read
		 */
		vmem_delete(vmp);
		vmp = vmem_create_in_region(addr + CHUNKSIZE, POOLSIZE);
		if (vmp == NULL)
			UT_FATAL("!vmem_create_in_region");
		do_iterate();
		vmem_delete(vmp);
		break;

	default:
		UT_FATAL("wrong test case %d", test);
	}

	MUNMAP(addr, VMEM_MIN_POOL + CHUNKSIZE);

	DONE(NULL);
}
