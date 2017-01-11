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
 * obj_realloc.c -- unit test for pmemobj_realloc and pmemobj_zrealloc
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"

#include "heap.h"
#include "obj.h"
#include "util.h"

#define MIN_ALLOC_SIZE	MIN_RUN_SIZE
#define MAX_ALLOC_SIZE	CHUNKSIZE
#define ALLOC_CLASS_MUL	RUN_UNIT_MAX_ALLOC
#define MAX_ALLOC_MUL	RUN_UNIT_MAX_ALLOC
#define MAX_ALLOC_CLASS	5
#define ALLOC_HDR	(OBJ_OOB_SIZE + sizeof(struct allocation_header))

POBJ_LAYOUT_BEGIN(realloc);
POBJ_LAYOUT_ROOT(realloc, struct root);
POBJ_LAYOUT_TOID(realloc, struct object);
POBJ_LAYOUT_END(realloc);

struct object {
	size_t value;
	char data[];
};

struct root {
	TOID(struct object) obj;
	char data[CHUNKSIZE - sizeof(TOID(struct object))];
};

static size_t sizes[MAX_ALLOC_CLASS];

/*
 * test_alloc -- test allocation using realloc
 */
static void
test_alloc(PMEMobjpool *pop, size_t size)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));

	int ret = pmemobj_realloc(pop, &D_RW(root)->obj.oid, size,
			TOID_TYPE_NUM(struct object));
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!TOID_IS_NULL(D_RO(root)->obj));
	UT_ASSERT(pmemobj_alloc_usable_size(D_RO(root)->obj.oid) >= size);
}

/*
 * test_free -- test free using realloc
 */
static void
test_free(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERT(!TOID_IS_NULL(D_RO(root)->obj));

	int ret = pmemobj_realloc(pop, &D_RW(root)->obj.oid, 0,
			TOID_TYPE_NUM(struct object));
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));
}

static int check_integrity = 1;

/*
 * fill_buffer -- fill buffer with random data and return its checksum
 */
static uint16_t
fill_buffer(unsigned char *buf, size_t size)
{
	for (size_t i = 0; i < size; ++i)
		buf[i] = rand() % 255;
	pmem_persist(buf, size);
	return ut_checksum(buf, size);
}

/*
 * test_realloc -- test single reallocation
 */
static void
test_realloc(PMEMobjpool *pop, size_t size_from, size_t size_to,
		unsigned type_from, unsigned type_to, int zrealloc)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));

	int ret;
	if (zrealloc)
		ret = pmemobj_zalloc(pop, &D_RW(root)->obj.oid,
			size_from, type_from);
	else
		ret = pmemobj_alloc(pop, &D_RW(root)->obj.oid,
			size_from, type_from, NULL, NULL);

	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!TOID_IS_NULL(D_RO(root)->obj));
	size_t usable_size_from =
			pmemobj_alloc_usable_size(D_RO(root)->obj.oid);

	UT_ASSERT(usable_size_from >= size_from);

	size_t check_size;
	uint16_t checksum;

	if (zrealloc) {
		UT_ASSERT(util_is_zeroed(D_RO(D_RO(root)->obj),
					size_from));
	} else if (check_integrity) {
		check_size = size_to >= usable_size_from ?
				usable_size_from : size_to;
		checksum = fill_buffer((unsigned char *)D_RW(D_RW(root)->obj),
				check_size);
	}

	if (zrealloc) {
		ret = pmemobj_zrealloc(pop, &D_RW(root)->obj.oid,
				size_to, type_to);
	} else {
		ret = pmemobj_realloc(pop, &D_RW(root)->obj.oid,
				size_to, type_to);
	}

	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!TOID_IS_NULL(D_RO(root)->obj));
	UT_ASSERT(pmemobj_alloc_usable_size(D_RO(root)->obj.oid) >= size_to);

	if (zrealloc) {
		UT_ASSERT(util_is_zeroed(D_RO(D_RO(root)->obj), size_to));
	} else if (check_integrity) {
		uint16_t checksum2 = ut_checksum(
				(uint8_t *)D_RW(D_RW(root)->obj), check_size);
		if (checksum2 != checksum)
			UT_ASSERTinfo(0, "memory corruption");
	}

	pmemobj_free(&D_RW(root)->obj.oid);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));
}

/*
 * test_realloc_sizes -- test reallocations from/to specified sizes
 */
static void
test_realloc_sizes(PMEMobjpool *pop, unsigned type_from,
		unsigned type_to, int zrealloc, int size_diff)
{
	for (int i = 0; i < MAX_ALLOC_CLASS; i++) {
		size_t size_from = sizes[i] - ALLOC_HDR - size_diff;

		for (int j = 2; j <= MAX_ALLOC_MUL; j++) {
			size_t inc_size_to = sizes[i] * j - ALLOC_HDR;
			test_realloc(pop, size_from, inc_size_to,
				type_from, type_to, zrealloc);

			size_t dec_size_to = sizes[i] / j;
			if (dec_size_to <= ALLOC_HDR)
				dec_size_to = ALLOC_HDR;
			else
				dec_size_to -= ALLOC_HDR;

			test_realloc(pop, size_from, dec_size_to,
				type_from, type_to, zrealloc);

			for (int k = 0; k < MAX_ALLOC_CLASS; k++) {
				size_t prev_size = sizes[k] - ALLOC_HDR;

				test_realloc(pop, size_from, prev_size,
					type_from, type_to, zrealloc);
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_realloc");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(realloc) != 1);

	if (argc < 2)
		UT_FATAL("usage: %s file [check_integrity]", argv[0]);

	PMEMobjpool *pop = pmemobj_open(argv[1], POBJ_LAYOUT_NAME(realloc));
	if (!pop)
		UT_FATAL("!pmemobj_open");

	if (argc >= 3)
		check_integrity = atoi(argv[2]);

	/* initialize sizes */
	for (int i = 0; i < MAX_ALLOC_CLASS - 1; i++)
		sizes[i] = i == 0 ? MIN_ALLOC_SIZE :
			sizes[i - 1] * ALLOC_CLASS_MUL;
	sizes[MAX_ALLOC_CLASS - 1] = MAX_ALLOC_SIZE;


	/* test alloc and free */
	test_alloc(pop, 16);
	test_free(pop);

	/* test realloc without changing type number */
	test_realloc_sizes(pop, 0, 0, 0, 0);
	/* test realloc with changing type number */
	test_realloc_sizes(pop, 0, 1, 0, 0);
	/* test zrealloc without changing type number... */
	test_realloc_sizes(pop, 0, 0, 1, 8);
	test_realloc_sizes(pop, 0, 0, 1, 0);
	/* test zrealloc with changing type number... */
	test_realloc_sizes(pop, 0, 1, 1, 8);
	test_realloc_sizes(pop, 0, 1, 1, 0);


	pmemobj_close(pop);

	DONE(NULL);
}
