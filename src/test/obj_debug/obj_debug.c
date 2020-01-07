// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * obj_debug.c -- unit test for debug features
 *
 * usage: obj_debug file operation [op_index]:...
 *
 * operations are 'f' or 'l' or 'r' or 'a' or 'n' or 's'
 *
 */
#include <stddef.h>
#include <stdlib.h>
#include <sys/param.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "layout_obj_debug"

TOID_DECLARE_ROOT(struct root);
TOID_DECLARE(struct tobj, 0);
TOID_DECLARE(struct int3_s, 1);

struct root {
	POBJ_LIST_HEAD(listhead, struct tobj) lhead, lhead2;
	uint32_t val;
};

struct tobj {
	POBJ_LIST_ENTRY(struct tobj) next;
};

struct int3_s {
	uint32_t i1;
	uint32_t i2;
	uint32_t i3;
};

typedef	void (*func)(PMEMobjpool *pop, void *sync, void *cond);

static void
test_FOREACH(const char *path)
{
	PMEMobjpool *pop = NULL;
	PMEMoid varoid, nvaroid;
	TOID(struct root) root;
	TOID(struct tobj) var, nvar;

#define COMMANDS_FOREACH()\
	do {\
	POBJ_FOREACH(pop, varoid) {}\
	POBJ_FOREACH_SAFE(pop, varoid, nvaroid) {}\
	POBJ_FOREACH_TYPE(pop, var) {}\
	POBJ_FOREACH_SAFE_TYPE(pop, var, nvar) {}\
	POBJ_LIST_FOREACH(var, &D_RW(root)->lhead, next) {}\
	POBJ_LIST_FOREACH_REVERSE(var, &D_RW(root)->lhead, next) {}\
	} while (0)

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	TOID_ASSIGN(root, pmemobj_root(pop, sizeof(struct root)));
	POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->lhead, next,
			sizeof(struct tobj), NULL, NULL);

	COMMANDS_FOREACH();
	TX_BEGIN(pop) {
		COMMANDS_FOREACH();
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
	COMMANDS_FOREACH();

	pmemobj_close(pop);
}

static void
test_lists(const char *path)
{
	PMEMobjpool *pop = NULL;
	TOID(struct root) root;
	TOID(struct tobj) elm;

#define COMMANDS_LISTS()\
	do {\
	POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->lhead, next,\
			sizeof(struct tobj), NULL, NULL);\
	POBJ_NEW(pop, &elm, struct tobj, NULL, NULL);\
	POBJ_LIST_INSERT_AFTER(pop, &D_RW(root)->lhead,\
			POBJ_LIST_FIRST(&D_RW(root)->lhead), elm, next);\
	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(root)->lhead,\
			&D_RW(root)->lhead2, elm, next, next);\
	POBJ_LIST_REMOVE(pop, &D_RW(root)->lhead2, elm, next);\
	POBJ_FREE(&elm);\
	} while (0)

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	TOID_ASSIGN(root, pmemobj_root(pop, sizeof(struct root)));

	COMMANDS_LISTS();
	TX_BEGIN(pop) {
		COMMANDS_LISTS();
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
	COMMANDS_LISTS();

	pmemobj_close(pop);
}

static int
int3_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct int3_s *args = (struct int3_s *)arg;
	struct int3_s *val = (struct int3_s *)ptr;

	val->i1 = args->i1;
	val->i2 = args->i2;
	val->i3 = args->i3;

	pmemobj_persist(pop, val, sizeof(*val));

	return 0;
}

static void
test_alloc_construct(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	TX_BEGIN(pop) {
		struct int3_s args = { 1, 2, 3 };
		PMEMoid allocation;
		pmemobj_alloc(pop, &allocation, sizeof(allocation), 1,
				int3_constructor, &args);
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	pmemobj_close(pop);
}

static void
test_double_free(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid oid, oid2;
	int err = pmemobj_zalloc(pop, &oid, 100, 0);
	UT_ASSERTeq(err, 0);
	UT_ASSERT(!OID_IS_NULL(oid));

	oid2 = oid;

	pmemobj_free(&oid);
	pmemobj_free(&oid2);
}

static int
test_constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	PMEMoid oid;
	pmemobj_alloc(pop, &oid, 1, 1, test_constr, NULL);

	return 0;
}

static void
test_alloc_in_constructor(const char *path)
{
	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	PMEMoid oid;
	pmemobj_alloc(pop, &oid, 1, 1, test_constr, NULL);
}

static void
test_mutex_lock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_mutex_lock(pop, (PMEMmutex *)sync);
}

static void
test_mutex_unlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_mutex_unlock(pop, (PMEMmutex *)sync);
}

static void
test_mutex_trylock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_mutex_trylock(pop, (PMEMmutex *)sync);
}

static void
test_mutex_timedlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_mutex_timedlock(pop, (PMEMmutex *)sync, NULL);
}

static void
test_mutex_zero(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_mutex_zero(pop, (PMEMmutex *)sync);
}

static void
test_rwlock_rdlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_rdlock(pop, (PMEMrwlock *)sync);
}

static void
test_rwlock_wrlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_wrlock(pop, (PMEMrwlock *)sync);
}

static void
test_rwlock_timedrdlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_timedrdlock(pop, (PMEMrwlock *)sync, NULL);
}

static void
test_rwlock_timedwrlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_timedwrlock(pop, (PMEMrwlock *)sync, NULL);
}

static void
test_rwlock_tryrdlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_tryrdlock(pop, (PMEMrwlock *)sync);
}

static void
test_rwlock_trywrlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_trywrlock(pop, (PMEMrwlock *)sync);
}

static void
test_rwlock_unlock(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_unlock(pop, (PMEMrwlock *)sync);
}

static void
test_rwlock_zero(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_rwlock_zero(pop, (PMEMrwlock *)sync);
}

static void
test_cond_wait(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_cond_wait(pop, (PMEMcond *)cond, (PMEMmutex *)sync);
}

static void
test_cond_signal(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_cond_signal(pop, (PMEMcond *)cond);
}

static void
test_cond_broadcast(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_cond_broadcast(pop, (PMEMcond *)cond);
}

static void
test_cond_timedwait(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_cond_timedwait(pop, (PMEMcond *)cond, (PMEMmutex *)sync, NULL);
}

static void
test_cond_zero(PMEMobjpool *pop, void *sync, void *cond)
{
	pmemobj_cond_zero(pop, (PMEMcond *)cond);
}

static void
test_sync_pop_check(unsigned long op_index)
{
	PMEMobjpool *pop = (PMEMobjpool *)(uintptr_t)0x1;

	func to_test[] = {
		test_mutex_lock, test_mutex_unlock, test_mutex_trylock,
		test_mutex_timedlock, test_mutex_zero, test_rwlock_rdlock,
		test_rwlock_wrlock, test_rwlock_timedrdlock,
		test_rwlock_timedwrlock, test_rwlock_tryrdlock,
		test_rwlock_trywrlock, test_rwlock_unlock, test_rwlock_zero,
		test_cond_wait, test_cond_signal, test_cond_broadcast,
		test_cond_timedwait, test_cond_zero
	};

	if (op_index >= (sizeof(to_test) / sizeof(to_test[0])))
		UT_FATAL("Invalid op_index provided");

	PMEMmutex stack_sync;
	PMEMcond stack_cond;

	to_test[op_index](pop, &stack_sync, &stack_cond);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_debug");

	if (argc < 3)
		UT_FATAL("usage: %s file-name op:f|l|r|a|s [op_index]",
				argv[0]);

	const char *path = argv[1];

	if (strchr("flrapns", argv[2][0]) == NULL || argv[2][1] != '\0')
		UT_FATAL("op must be f or l or r or a or p or n or s");

	unsigned long op_index;
	char *tailptr;

	switch (argv[2][0]) {
		case 'f':
			test_FOREACH(path);
			break;
		case 'l':
			test_lists(path);
			break;
		case 'a':
			test_alloc_construct(path);
			break;
		case 'p':
			test_double_free(path);
			break;
		case 'n':
			test_alloc_in_constructor(path);
			break;
		case 's':
			if (argc != 4)
				UT_FATAL("Provide an op_index with option s");
			op_index = strtoul(argv[3], &tailptr, 10);
			if (tailptr[0] != '\0')
				UT_FATAL("Wrong op_index format");

			test_sync_pop_check(op_index);
			break;
	}

	DONE(NULL);
}
