/*
 * Copyright 2018, Intel Corporation
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
 * obj_memops.c -- basic memory operations tests
 *
 */

#include <stddef.h>
#include "obj.h"
#include "memops.h"
#include "unittest.h"

#define TEST_ENTRIES 16

#define TEST_VALUES 128

enum fail_types {
	FAIL_NONE,
	FAIL_CHECKSUM,
	FAIL_MODIFY_NEXT,
	FAIL_MODIFY_VALUE,
};

struct test_object {
	struct REDO_LOG(TEST_ENTRIES) redo;
	uint64_t values[TEST_VALUES];
};

static void
clear_test_values(struct test_object *object)
{
	memset(object->values, 0, sizeof(uint64_t) * TEST_VALUES);
}

static int
redo_log_constructor(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = ctx;
	const struct pmem_ops *p_ops = &pop->p_ops;

	struct redo_log *redo = ptr;
	redo->capacity = TEST_ENTRIES;
	redo->checksum = 0;
	redo->next = 0;
	memset(redo->unused, 0, sizeof(redo->unused));

	pmemops_flush(p_ops, redo, sizeof(*redo));

	pmemops_memset(p_ops, redo->entries, 0,
		usable_size - sizeof(*redo), 0);

	return 0;
}

static int
pmalloc_redo_extend(void *base, uint64_t *redo)
{
	size_t s = SIZEOF_REDO_LOG(TEST_ENTRIES);

	return pmalloc_construct(base, redo, s, redo_log_constructor, NULL, 0,
		OBJ_INTERNAL_OBJECT_MASK, 0);
}

static void
test_set_entries(PMEMobjpool *pop,
	struct operation_context *ctx, struct test_object *object,
	size_t nentries, enum fail_types fail)
{
	operation_start(ctx);

	for (size_t i = 0; i < nentries; ++i) {
		operation_add_typed_entry(ctx,
			&object->values[i], i + 1,
			REDO_OPERATION_SET, LOG_PERSISTENT);
	}

	operation_reserve(ctx, nentries);

	if (fail != FAIL_NONE) {
		operation_cancel(ctx);

		switch (fail) {
			case FAIL_CHECKSUM:
				object->redo.checksum += 1;
			break;
			case FAIL_MODIFY_NEXT:
				pmalloc_redo_extend(pop,
					&object->redo.next);
			break;
			case FAIL_MODIFY_VALUE:
				object->redo.entries[1].offset += 8;
			break;
			default:
				UT_ASSERT(0);
		}

		redo_log_recover(pop->redo, (struct redo_log *)&object->redo);

		for (size_t i = 0; i < nentries; ++i)
			UT_ASSERTeq(object->values[i], 0);
	} else {
		operation_process(ctx);

		for (size_t i = 0; i < nentries; ++i)
			UT_ASSERTeq(object->values[i], i + 1);
	}
}

static void
test_same_twice(struct operation_context *ctx, struct test_object *object)
{
	operation_start(ctx);

	operation_add_typed_entry(ctx,
		&object->values[0], 5,
		REDO_OPERATION_SET, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 10,
		REDO_OPERATION_SET, LOG_PERSISTENT);

	operation_process(ctx);

	UT_ASSERTeq(object->values[0], 5);
}

static void
test_merge_op(struct operation_context *ctx, struct test_object *object)
{
	operation_start(ctx);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b10,
		REDO_OPERATION_OR, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b01,
		REDO_OPERATION_OR, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b00,
		REDO_OPERATION_AND, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b01,
		REDO_OPERATION_OR, LOG_PERSISTENT);

	operation_process(ctx);

	UT_ASSERTeq(object->values[0], 0b01);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_memops");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, "obj_memops",
			PMEMOBJ_MIN_POOL * 10, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	struct test_object *object =
		pmemobj_direct(pmemobj_root(pop, sizeof(struct test_object)));
	UT_ASSERTne(object, NULL);

	struct operation_context *ctx = operation_new(pop, pop->redo,
		(struct redo_log *)&object->redo, TEST_ENTRIES,
		pmalloc_redo_extend);

	test_set_entries(pop, ctx, object, 10, FAIL_NONE);
	clear_test_values(object);
	test_same_twice(ctx, object);
	clear_test_values(object);
	test_merge_op(ctx, object);
	clear_test_values(object);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_NONE);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_CHECKSUM);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 10, FAIL_CHECKSUM);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_MODIFY_VALUE);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 10, FAIL_MODIFY_VALUE);

	operation_delete(ctx);

	/* verify that rebuilding redo_next works */
	ctx = operation_new(pop, pop->redo,
		(struct redo_log *)&object->redo, TEST_ENTRIES,
		NULL);

	test_set_entries(pop, ctx, object, 100, 0);
	clear_test_values(object);

	/* FAIL_MODIFY_NEXT tests can only happen after redo_next test */
	test_set_entries(pop, ctx, object, 100, FAIL_MODIFY_NEXT);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 10, FAIL_MODIFY_NEXT);
	clear_test_values(object);

	operation_delete(ctx);

	pmemobj_close(pop);

	DONE(NULL);
}

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
