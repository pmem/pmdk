// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2022, Intel Corporation */

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

#define TEST_VALUES TEST_ENTRIES

enum fail_types {
	FAIL_NONE,
	FAIL_CHECKSUM,
	FAIL_MODIFY_NEXT,
	FAIL_MODIFY_VALUE,
};

struct test_object {
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

	size_t capacity = ALIGN_DOWN(usable_size - sizeof(struct ulog),
					CACHELINE_SIZE);

	ulog_construct(OBJ_PTR_TO_OFF(ctx, ptr), capacity,
			*(uint64_t *)arg, 1, 0, p_ops);

	return 0;
}

static int
pmalloc_redo_extend(void *base, uint64_t *redo, uint64_t gen_num)
{
	size_t s = SIZEOF_ALIGNED_ULOG(TEST_ENTRIES);

	return pmalloc_construct(base, redo, s, redo_log_constructor, &gen_num,
		0, OBJ_INTERNAL_OBJECT_MASK, 0);
}

static void
test_free_entry(void *base, uint64_t *next)
{
	*next = 0;
	/* noop for fake ulog entries */
}

static void
test_set_entries(PMEMobjpool *pop,
	struct operation_context *ctx, struct test_object *object,
	size_t nentries, enum fail_types fail, enum operation_log_type type)
{
	operation_start(ctx);

	UT_ASSERT(nentries <= ARRAY_SIZE(object->values));

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
					&object->redo.next, 0);
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
		operation_process(ctx);
		operation_finish(ctx, 0);

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

	operation_process(ctx);
	operation_finish(ctx, 0);

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
		pmalloc_redo_extend, (ulog_free_fn)pfree,
		&pop->p_ops, LOG_TYPE_REDO);

	/*
	 * Keep this test first.
	 * It tests a situation where the number of objects being added
	 * is equal to the capacity of the log.
	 */
	test_set_entries(pop, ctx, object, TEST_ENTRIES - 1,
		FAIL_NONE, LOG_PERSISTENT);
	clear_test_values(object);

	test_set_entries(pop, ctx, object, 100, FAIL_NONE, LOG_TRANSIENT);
	clear_test_values(object);
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
	operation_delete(ctx);

	/*
	 * Verify that rebuilding redo_next works. This requires that
	 * object->redo->next is != 0 - to achieve that, this test must
	 * be preceded by a test that fails to finish the ulog's operation.
	 */
	ctx = operation_new(
		(struct ulog *)&object->redo, TEST_ENTRIES,
		NULL, test_free_entry, &pop->p_ops, LOG_TYPE_REDO);

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
	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);

	operation_start(ctx);

	UT_ASSERTeq(object->values[0], 1);
	UT_ASSERTeq(object->values[1], 2);

	object->values[0] = 2;
	object->values[1] = 1;

	operation_process(ctx);

	UT_ASSERTeq(object->values[0], 2);
	UT_ASSERTeq(object->values[1], 1);

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);
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

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);
}

static void
test_undo_small_multiple_set(struct operation_context *ctx,
	struct test_object *object)
{
	operation_start(ctx);

	object->values[0] = 1;
	object->values[1] = 2;

	int c = 0;

	operation_add_buffer(ctx,
		&object->values[0], &c, sizeof(*object->values),
		ULOG_OPERATION_BUF_SET);
	operation_add_buffer(ctx,
		&object->values[1], &c, sizeof(*object->values),
		ULOG_OPERATION_BUF_SET);

	operation_process(ctx);

	UT_ASSERTeq(object->values[0], 0);
	UT_ASSERTeq(object->values[1], 0);

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);
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

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);
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
	pmemobj_persist(pop, &log->data[100], sizeof(log->data[100]));

	operation_process(ctx);

	/* the log shouldn't get applied */
	for (uint64_t i = 0; i < 20; ++i)
		UT_ASSERTeq(object->values[i], i + 2);

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);
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

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);

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

	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM);
}

static int
test_undo_foreach(struct ulog_entry_base *e, void *arg,
	const struct pmem_ops *p_ops)
{
	size_t *nentries = arg;
	++(*nentries);

	return 0;
}

/*
 * drain_empty -- drain for pmem_ops
 */
static void
drain_empty(void *ctx)
{
	/* do nothing */
}

/*
 * persist_empty -- persist for pmem_ops
 */
static int
persist_empty(void *ctx, const void *addr, size_t len, unsigned flags)
{
	return 0;
}

/*
 * flush_empty -- flush for pmem_ops
 */
static int
flush_empty(void *ctx, const void *addr, size_t len, unsigned flags)
{
	return 0;
}

/*
 * memcpy_libc -- memcpy for pmem_ops
 */
static void *
memcpy_libc(void *ctx, void *dest, const void *src, size_t len, unsigned flags)
{
	return memcpy(dest, src, len);
}

/*
 * memset_libc -- memset for pmem_ops
 */
static void *
memset_libc(void *ctx, void *ptr, int c, size_t sz, unsigned flags)
{
	return memset(ptr, c, sz);
}

/*
 * test_undo_log_reuse -- test for correct reuse of log space
 */
static void
test_undo_log_reuse()
{
#define ULOG_SIZE 1024
	struct pmem_ops ops = {
		.persist = persist_empty,
		.flush = flush_empty,
		.drain = drain_empty,
		.memcpy = memcpy_libc,
		.memmove = NULL,
		.memset = memset_libc,
		.base = NULL,
	};
	struct ULOG(ULOG_SIZE) *first = util_aligned_malloc(CACHELINE_SIZE,
		SIZEOF_ULOG(ULOG_SIZE));
	struct ULOG(ULOG_SIZE) *second = util_aligned_malloc(CACHELINE_SIZE,
		SIZEOF_ULOG(ULOG_SIZE));
	ulog_construct((uint64_t)(first), ULOG_SIZE, 0, 0, 0, &ops);
	ulog_construct((uint64_t)(second), ULOG_SIZE, 0, 0, 0, &ops);

	first->next = (uint64_t)(second);

	struct operation_context *ctx = operation_new(
		(struct ulog *)first, ULOG_SIZE,
		NULL, test_free_entry,
		&ops, LOG_TYPE_UNDO);

	size_t nentries = 0;
	ulog_foreach_entry((struct ulog *)first,
		test_undo_foreach, &nentries, &ops);
	UT_ASSERTeq(nentries, 0);

	/* first, let's populate the log with some valid entries */

	size_t entry_size = (ULOG_SIZE / 2) - sizeof(struct ulog_entry_buf);
	size_t total_entries = ((ULOG_SIZE * 2) / entry_size);
	char *data = MALLOC(entry_size);
	memset(data, 0xc, entry_size); /* fill it with something */

	for (size_t i = 0; i < total_entries; ++i) {
		operation_add_buffer(ctx, (void *)0x123, data,
			entry_size,
			ULOG_OPERATION_BUF_CPY);

		nentries = 0;
		ulog_foreach_entry((struct ulog *)first,
			test_undo_foreach, &nentries, &ops);
		UT_ASSERTeq(nentries, i + 1);
	}

	operation_init(ctx); /* initialize a new operation */

	/* let's overwrite old entries and see if they are no longer visible */
	for (size_t i = 0; i < total_entries; ++i) {
		operation_add_buffer(ctx, (void *)0x123, data,
			entry_size,
			ULOG_OPERATION_BUF_CPY);

		nentries = 0;
		ulog_foreach_entry((struct ulog *)first,
			test_undo_foreach, &nentries, &ops);
		UT_ASSERTeq(nentries, i + 1);
	}

	FREE(data);
	operation_delete(ctx);
	util_aligned_free(first);
	util_aligned_free(second);
#undef ULOG_SIZE
}

/*
 * test_undo_log_reuse -- test for correct reuse of log space
 */
static void
test_undo_log_resume()
{
#define ULOG_SIZE 1024
	struct pmem_ops ops = {
		.persist = persist_empty,
		.flush = flush_empty,
		.drain = drain_empty,
		.memcpy = memcpy_libc,
		.memmove = NULL,
		.memset = memset_libc,
		.base = NULL,
	};
	struct ULOG(ULOG_SIZE) *first = util_aligned_malloc(CACHELINE_SIZE,
		SIZEOF_ULOG(ULOG_SIZE));
	struct ULOG(ULOG_SIZE) *second = util_aligned_malloc(CACHELINE_SIZE,
		SIZEOF_ULOG(ULOG_SIZE));
	ulog_construct((uint64_t)(first), ULOG_SIZE, 0, 0, 0, &ops);
	ulog_construct((uint64_t)(second), ULOG_SIZE, 0, 0, 0, &ops);

	first->next = (uint64_t)(second);

	struct operation_context *ctx = operation_new(
		(struct ulog *)first, ULOG_SIZE,
		NULL, test_free_entry,
		&ops, LOG_TYPE_UNDO);

	/* first, let's populate the log with some valid entries */
	size_t entry_size = (ULOG_SIZE / 2) - sizeof(struct ulog_entry_buf);
	size_t total_entries = ((ULOG_SIZE * 2) / entry_size);
	char *data = MALLOC(entry_size);
	memset(data, 0xc, entry_size); /* fill it with something */

	size_t nentries = 0;
	for (size_t i = 0; i < total_entries; ++i) {
		operation_add_buffer(ctx, (void *)0x123, data,
			entry_size,
			ULOG_OPERATION_BUF_CPY);

		nentries = 0;
		ulog_foreach_entry((struct ulog *)first,
			test_undo_foreach, &nentries, &ops);
		UT_ASSERTeq(nentries, i + 1);
	}
	/* break the log so that it can't be processed */
	first->gen_num = 1;

	/* resume and process the operation */
	operation_resume(ctx);
	operation_process(ctx);
	operation_finish(ctx, ULOG_INC_FIRST_GEN_NUM |
			ULOG_FREE_AFTER_FIRST);

	/*
	 * The resumed log should continue to be functional, but with only
	 * the first log.
	 */
	for (size_t i = 0; i < total_entries / 2; ++i) {
		operation_add_buffer(ctx, (void *)0x123, data,
			entry_size,
			ULOG_OPERATION_BUF_CPY);

		nentries = 0;
		ulog_foreach_entry((struct ulog *)first,
			test_undo_foreach, &nentries, &ops);
		UT_ASSERTeq(nentries, i + 1);
	}

	FREE(data);
	operation_delete(ctx);
	util_aligned_free(first);
	util_aligned_free(second);
#undef ULOG_SIZE
}

/*
 * test_undo_log_reuse -- test for correct reuse of log space
 */
static void
test_redo_cleanup_same_size(PMEMobjpool *pop, struct test_object *object)
{
#define ULOG_SIZE 1024
	struct operation_context *ctx = operation_new(
		(struct ulog *)&object->redo, TEST_ENTRIES,
		pmalloc_redo_extend, (ulog_free_fn)pfree,
		&pop->p_ops, LOG_TYPE_REDO);

	int ret = pmalloc(pop, &object->redo.next, ULOG_SIZE, 0, 0);
	UT_ASSERTeq(ret, 0);

	/* undo logs are clobbered at the end, which shrinks their size */
	size_t capacity = ulog_capacity((struct ulog *)&object->undo,
		TEST_ENTRIES, &pop->p_ops);

	/* builtin log + one next */
	UT_ASSERTeq(capacity, TEST_ENTRIES * 2 + CACHELINE_SIZE);

	operation_start(ctx); /* initialize a new operation */

	struct pobj_action act;
	pmemobj_reserve(pop, &act, ULOG_SIZE, 0);
	palloc_publish(&pop->heap, &act, 1, ctx);

	operation_delete(ctx);
#undef ULOG_SIZE
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
	test_undo_small_multiple_set(ctx, object);
	test_undo_large_single_copy(ctx, object);
	test_undo_large_copy(pop, ctx, object);
	test_undo_checksum_mismatch(pop, ctx, object,
		(struct ulog *)&object->undo);

	/* undo logs are clobbered at the end, which shrinks their size */
	size_t capacity = ulog_capacity((struct ulog *)&object->undo,
		TEST_ENTRIES, &pop->p_ops);

	/* builtin log + one next */
	UT_ASSERTeq(capacity, TEST_ENTRIES * 2 + CACHELINE_SIZE);

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

	/*
	 * The ulog API requires cacheline alignment. A cacheline aligned new
	 * new allocator is created here to properly test the ulog api.
	 * A aligned object can then be allocated using pmemobj_xalloc.
	 */
	struct pobj_alloc_class_desc new_ac = {
		.unit_size = sizeof(struct test_object),
		.alignment = CACHELINE_SIZE,
		.units_per_block = 1,
		.header_type = POBJ_HEADER_NONE,
	};
	if (pmemobj_ctl_set(pop, "heap.alloc_class.new.desc", &new_ac) == -1)
		UT_FATAL("Failed to set allocation class");

	PMEMoid pobject;
	if (pmemobj_xalloc(pop, &pobject, sizeof(struct test_object), 0,
		POBJ_CLASS_ID(new_ac.class_id), NULL, NULL) == -1)
		UT_FATAL("Failed to allocate object");

	struct test_object *object = pmemobj_direct(pobject);

	UT_ASSERTne(object, NULL);
	ulog_construct(OBJ_PTR_TO_OFF(pop, &object->undo),
			TEST_ENTRIES, 0, 0, 0, &pop->p_ops);
	ulog_construct(OBJ_PTR_TO_OFF(pop, &object->redo),
			TEST_ENTRIES, 0, 0, 0, &pop->p_ops);

	test_redo(pop, object);
	test_undo(pop, object);
	test_redo_cleanup_same_size(pop, object);
	test_undo_log_reuse();
	test_undo_log_resume();

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
