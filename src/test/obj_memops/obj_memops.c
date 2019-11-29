/*
 * Copyright 2019, Intel Corporation
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
#include "ulog.h"
#include "unittest.h"

#define TEST_ENTRIES 256

#define TEST_VALUES 128

enum fail_types {
	FAIL_NONE,
	FAIL_CHECKSUM,
	FAIL_MODIFY_NEXT,
	FAIL_MODIFY_VALUE,
};

struct test_object {
	uint8_t padding[CACHELINE_SIZE - 16]; /* align to a cacheline */
	struct ULOG(TEST_ENTRIES) redo;
	struct ULOG(TEST_ENTRIES) undo;
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

	ulog_construct(OBJ_PTR_TO_OFF(ctx, ptr), TEST_ENTRIES, 1, p_ops);

	return 0;
}

static int
pmalloc_redo_extend(void *base, uint64_t *redo)
{
	size_t s = SIZEOF_ULOG(TEST_ENTRIES);

	return pmalloc_construct(base, redo, s, redo_log_constructor, NULL, 0,
		OBJ_INTERNAL_OBJECT_MASK, 0);
}

static void
test_set_entries(PMEMobjpool *pop,
	struct operation_context *ctx, struct test_object *object,
	size_t nentries, enum fail_types fail, enum operation_log_type type)
{
	operation_start(ctx);

	for (size_t i = 0; i < nentries; ++i) {
		operation_add_typed_entry(ctx,
			&object->values[i], i + 1,
			ULOG_OPERATION_SET, type);
	}

	operation_reserve(ctx, nentries * 16);

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
				object->redo.data[16] += 8;
			break;
			default:
				UT_ASSERT(0);
		}

		ulog_recover((struct ulog *)&object->redo,
			OBJ_OFF_IS_VALID_FROM_CTX, &pop->p_ops);

		for (size_t i = 0; i < nentries; ++i)
			UT_ASSERTeq(object->values[i], 0);
	} else {
		operation_finish(ctx);

		for (size_t i = 0; i < nentries; ++i)
			UT_ASSERTeq(object->values[i], i + 1);
	}
}

static void
test_merge_op(struct operation_context *ctx, struct test_object *object)
{
	operation_start(ctx);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b10,
		ULOG_OPERATION_OR, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b01,
		ULOG_OPERATION_OR, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b00,
		ULOG_OPERATION_AND, LOG_PERSISTENT);

	operation_add_typed_entry(ctx,
		&object->values[0], 0b01,
		ULOG_OPERATION_OR, LOG_PERSISTENT);

	operation_finish(ctx);

	UT_ASSERTeq(object->values[0], 0b01);
}

static void
test_same_twice(struct operation_context *ctx, struct test_object *object)
{
	operation_start(ctx);
	operation_add_typed_entry(ctx,
		&object->values[0], 5,
		ULOG_OPERATION_SET, LOG_PERSISTENT);
	operation_add_typed_entry(ctx,
		&object->values[0], 10,
		ULOG_OPERATION_SET, LOG_PERSISTENT);
	operation_process(ctx);
	UT_ASSERTeq(object->values[0], 10);
	operation_cancel(ctx);
}

static void
test_redo(PMEMobjpool *pop, struct test_object *object)
{
	struct operation_context *ctx = operation_new(
		(struct ulog *)&object->redo, TEST_ENTRIES,
		pmalloc_redo_extend, NULL, &pop->p_ops, LOG_TYPE_REDO);

	test_set_entries(pop, ctx, object, 10, FAIL_NONE, LOG_PERSISTENT);
	clear_test_values(object);
	test_merge_op(ctx, object);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_NONE, LOG_PERSISTENT);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_CHECKSUM, LOG_PERSISTENT);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 10, FAIL_CHECKSUM, LOG_PERSISTENT);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_MODIFY_VALUE,
		LOG_PERSISTENT);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 10, FAIL_MODIFY_VALUE,
		LOG_PERSISTENT);
	clear_test_values(object);
	test_same_twice(ctx, object);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 100, FAIL_NONE, LOG_TRANSIENT);
	clear_test_values(object);

	operation_delete(ctx);

	/* verify that rebuilding redo_next works */
	ctx = operation_new(
		(struct ulog *)&object->redo, TEST_ENTRIES,
		NULL, NULL, &pop->p_ops, LOG_TYPE_REDO);

	test_set_entries(pop, ctx, object, 100, 0, LOG_PERSISTENT);
	clear_test_values(object);

	/* FAIL_MODIFY_NEXT tests can only happen after redo_next test */
	test_set_entries(pop, ctx, object, 100, FAIL_MODIFY_NEXT,
		LOG_PERSISTENT);
	clear_test_values(object);
	test_set_entries(pop, ctx, object, 10, FAIL_MODIFY_NEXT,
		LOG_PERSISTENT);
	clear_test_values(object);

	operation_delete(ctx);
}

static void
test_undo_small_single_copy(struct operation_context *ctx,
	struct test_object *object)
{
	operation_start(ctx);

	object->values[0] = 1;
	object->values[1] = 2;

	operation_add_buffer(ctx,
		&object->values, &object->values, sizeof(*object->values) * 2,
		ULOG_OPERATION_BUF_CPY);

	object->values[0] = 2;
	object->values[1] = 1;

	operation_process(ctx);
	operation_finish(ctx);

	operation_start(ctx);

	UT_ASSERTeq(object->values[0], 1);
	UT_ASSERTeq(object->values[1], 2);

	object->values[0] = 2;
	object->values[1] = 1;

	operation_process(ctx);

	UT_ASSERTeq(object->values[0], 2);
	UT_ASSERTeq(object->values[1], 1);

	operation_finish(ctx);
}

static void
test_undo_small_single_set(struct operation_context *ctx,
	struct test_object *object)
{
	operation_start(ctx);

	object->values[0] = 1;
	object->values[1] = 2;

	int c = 0;

	operation_add_buffer(ctx,
		&object->values, &c, sizeof(*object->values) * 2,
		ULOG_OPERATION_BUF_SET);

	operation_process(ctx);

	UT_ASSERTeq(object->values[0], 0);
	UT_ASSERTeq(object->values[1], 0);

	operation_finish(ctx);
}

static void
test_undo_large_single_copy(struct operation_context *ctx,
	struct test_object *object)
{
	operation_start(ctx);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		object->values[i] = i + 1;

	operation_add_buffer(ctx,
		&object->values, &object->values, sizeof(object->values),
		ULOG_OPERATION_BUF_CPY);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		object->values[i] = i + 2;

	operation_process(ctx);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		UT_ASSERTeq(object->values[i], i + 1);

	operation_finish(ctx);
}

static void
test_undo_checksum_mismatch(PMEMobjpool *pop, struct operation_context *ctx,
	struct test_object *object, struct ulog *log)
{
	operation_start(ctx);

	for (uint64_t i = 0; i < 20; ++i)
		object->values[i] = i + 1;

	operation_add_buffer(ctx,
		&object->values, &object->values, sizeof(*object->values) * 20,
		ULOG_OPERATION_BUF_CPY);

	for (uint64_t i = 0; i < 20; ++i)
		object->values[i] = i + 2;

	pmemobj_persist(pop, &object->values, sizeof(*object->values) * 20);

	log->data[100] += 1; /* corrupt the log somewhere */

	operation_process(ctx);

	/* the log shouldn't get applied */
	for (uint64_t i = 0; i < 20; ++i)
		UT_ASSERTeq(object->values[i], i + 2);

	operation_finish(ctx);
}

static void
test_undo_large_copy(PMEMobjpool *pop, struct operation_context *ctx,
	struct test_object *object)
{
	operation_start(ctx);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		object->values[i] = i + 1;

	operation_add_buffer(ctx,
		&object->values, &object->values, sizeof(object->values),
		ULOG_OPERATION_BUF_CPY);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		object->values[i] = i + 2;

	operation_process(ctx);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		UT_ASSERTeq(object->values[i], i + 1);


	operation_finish(ctx);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		object->values[i] = i + 3;

	operation_start(ctx);

	operation_add_buffer(ctx,
		&object->values, &object->values, sizeof(*object->values) * 26,
		ULOG_OPERATION_BUF_CPY);

	for (uint64_t i = 0; i < TEST_VALUES; ++i)
		object->values[i] = i + 4;

	pmemobj_persist(pop, &object->values, sizeof(object->values));

	operation_process(ctx);

	for (uint64_t i = 0; i < 26; ++i)
		UT_ASSERTeq(object->values[i], i + 3);

	for (uint64_t i = 26; i < TEST_VALUES; ++i)
		UT_ASSERTeq(object->values[i], i + 4);

	operation_finish(ctx);
}

static void
test_undo(PMEMobjpool *pop, struct test_object *object)
{
	struct operation_context *ctx = operation_new(
		(struct ulog *)&object->undo, TEST_ENTRIES,
		pmalloc_redo_extend, (ulog_free_fn)pfree,
		&pop->p_ops, LOG_TYPE_UNDO);

	test_undo_small_single_copy(ctx, object);
	test_undo_small_single_set(ctx, object);
	test_undo_large_single_copy(ctx, object);
	test_undo_large_copy(pop, ctx, object);
	test_undo_checksum_mismatch(pop, ctx, object,
		(struct ulog *)&object->undo);

	/* undo logs are clobbered at the end, which shrinks their size */
	size_t capacity = ulog_capacity((struct ulog *)&object->undo,
		TEST_ENTRIES, &pop->p_ops);

	/* builtin log + one next */
	UT_ASSERTeq(capacity, TEST_ENTRIES * 2);

	operation_delete(ctx);
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
	ulog_construct(OBJ_PTR_TO_OFF(pop, &object->undo),
		TEST_ENTRIES, 1, &pop->p_ops);

	test_redo(pop, object);
	test_undo(pop, object);

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
