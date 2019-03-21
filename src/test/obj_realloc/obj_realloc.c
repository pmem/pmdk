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
 * obj_realloc.c -- unit test for pmemobj_realloc and pmemobj_zrealloc
 */
#include <sys/param.h>
#include <string.h>

#include "unittest.h"

#include "heap.h"
#include "alloc_class.h"
#include "obj.h"
#include "util.h"

#define MAX_ALLOC_MUL	8
#define MAX_ALLOC_CLASS	5

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

static struct alloc_class_collection *alloc_classes;

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

/*
 * test_huge_size -- test zrealloc with size greater than pool size
 */
static void
test_huge_size(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));

	int ret;

	ret = pmemobj_zrealloc(pop, &D_RW(root)->obj.oid,
		PMEMOBJ_MAX_ALLOC_SIZE, TOID_TYPE_NUM(struct object));
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));

	ret = pmemobj_zrealloc(pop, &D_RW(root)->obj.oid, UINTMAX_MAX,
		TOID_TYPE_NUM(struct object));
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));

	ret = pmemobj_zrealloc(pop, &D_RW(root)->obj.oid, UINTMAX_MAX - 1,
		TOID_TYPE_NUM(struct object));
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));
}

/* test zrealloc passing PMEMoid that points to OID_NULL value */
static void
test_null_oid(PMEMobjpool *pop)
{
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->obj));

	int ret = pmemobj_zrealloc(pop, &D_RW(root)->obj.oid, 1024,
		TOID_TYPE_NUM(struct object));
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!TOID_IS_NULL(D_RO(root)->obj));

	pmemobj_free(&D_RW(root)->obj.oid);
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
	uint64_t type_from, uint64_t type_to, int zrealloc)
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
	size_t usable_size_to =
			pmemobj_alloc_usable_size(D_RO(root)->obj.oid);

	UT_ASSERT(usable_size_to >= size_to);
	if (size_to < size_from) {
		UT_ASSERT(usable_size_to <= usable_size_from);
	}

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
test_realloc_sizes(PMEMobjpool *pop, uint64_t type_from,
	uint64_t type_to, int zrealloc, unsigned size_diff)
{
	for (uint8_t i = 0; i < MAX_ALLOCATION_CLASSES; ++i) {
		struct alloc_class *c = alloc_class_by_id(alloc_classes, i);
		if (c == NULL)
			continue;

		size_t header_size = header_type_to_size[c->header_type];
		size_t size_from = c->unit_size - header_size - size_diff;

		for (unsigned j = 2; j <= MAX_ALLOC_MUL; j++) {
			size_t inc_size_to = c->unit_size * j - header_size;
			test_realloc(pop, size_from, inc_size_to,
				type_from, type_to, zrealloc);

			size_t dec_size_to = c->unit_size / j;
			if (dec_size_to <= header_size)
				dec_size_to = header_size;
			else
				dec_size_to -= header_size;

			test_realloc(pop, size_from, dec_size_to,
				type_from, type_to, zrealloc);

			for (int k = 0; k < MAX_ALLOC_CLASS; k++) {
				struct alloc_class *ck = alloc_class_by_id(
					alloc_classes, k);
				if (c == NULL)
					continue;
				size_t header_sizek =
					header_type_to_size[c->header_type];
				size_t prev_size = ck->unit_size - header_sizek;

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

	alloc_classes = alloc_class_collection_new();

	/* test huge size alloc */
	test_huge_size(pop);

	/* test alloc and free */
	test_alloc(pop, 16);
	test_free(pop);

	/* test zrealloc passing PMEMoid that points to OID_NULL value */
	test_null_oid(pop);

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
	/* test realloc with type number equal to range of long long int */
	test_realloc_sizes(pop, 0, UINT64_MAX, 0, 0);
	test_realloc_sizes(pop, 0, UINT64_MAX - 1, 0, 0);
	/* test zrealloc with type number equal to range of long long int */
	test_realloc_sizes(pop, 0, UINT64_MAX, 1, 0);
	test_realloc_sizes(pop, 0, (UINT64_MAX - 1), 1, 0);

	alloc_class_collection_delete(alloc_classes);

	pmemobj_close(pop);

	DONE(NULL);
}

#ifdef _MSC_VER
extern "C" {
	/*
	 * Since libpmemobj is linked statically,
	 * we need to invoke its ctor/dtor.
	 */
	MSVC_CONSTR(libpmemobj_init)
	MSVC_DESTR(libpmemobj_fini)
}
#endif
