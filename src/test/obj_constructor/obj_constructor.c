// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2022, Intel Corporation */

/*
 * obj_constructor.c -- tests for constructor
 */

#include <stddef.h>

#include "unittest.h"

/*
 * Command line toggle indicating use of a bigger node structure for querying
 * pool size expressed in a number of possible allocations. A small node
 * structure results in a great number of allocations impossible to replicate
 * in assumed timeout.
 */
#define USE_BIG_ALLOC "--big-alloc"

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(constr);
POBJ_LAYOUT_ROOT(constr, struct root);
POBJ_LAYOUT_TOID(constr, struct node);
POBJ_LAYOUT_TOID(constr, struct node_big);
POBJ_LAYOUT_END(constr);

struct root {
	TOID(struct node) n;
	POBJ_LIST_HEAD(head, struct node) list;
	POBJ_LIST_HEAD(head_big, struct node_big) list_big;
};

struct node {
	POBJ_LIST_ENTRY(struct node) next;
};

struct node_big {
	POBJ_LIST_ENTRY(struct node_big) next;
	int weight[2048];
};

static int
root_constr_cancel(PMEMobjpool *pop, void *ptr, void *arg)
{
	return 1;
}

static int
node_constr_cancel(PMEMobjpool *pop, void *ptr, void *arg)
{
	return 1;
}

struct foo {
	int bar;
};

static struct foo *Canceled_ptr;

static int
vg_test_save_ptr(PMEMobjpool *pop, void *ptr, void *arg)
{
	Canceled_ptr = (struct foo *)ptr;
	return 1;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_constructor");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(constr) != 2);

	int big = (argc == 3 && strcmp(argv[2], USE_BIG_ALLOC) == 0);
	size_t node_size;
	size_t next_off;

	if (big) {
		node_size = sizeof(struct node_big);
		next_off = offsetof(struct node_big, next);
	} else if (argc == 2) {
		node_size = sizeof(struct node);
		next_off = offsetof(struct node, next);
	} else {
		UT_FATAL("usage: %s file-name [ %s ]", argv[0], USE_BIG_ALLOC);
	}

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	int ret;
	TOID(struct root) root;
	TOID(struct node) node;
	TOID(struct node_big) node_big;

	if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(constr),
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	errno = 0;
	root.oid = pmemobj_root_construct(pop, sizeof(struct root),
			root_constr_cancel, NULL);
	UT_ASSERT(TOID_IS_NULL(root));
	UT_ASSERTeq(errno, ECANCELED);

	/*
	 * Allocate memory until OOM, so we can check later if the alloc
	 * cancellation didn't damage the heap in any way.
	 */
	int allocs = 0;
	while (pmemobj_alloc(pop, NULL, node_size, 1, NULL, NULL) == 0)
		allocs++;

	UT_ASSERTne(allocs, 0);

	PMEMoid oid;
	PMEMoid next;
	POBJ_FOREACH_SAFE(pop, oid, next)
		pmemobj_free(&oid);

	errno = 0;
	ret = pmemobj_alloc(pop, NULL, node_size, 1, node_constr_cancel, NULL);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(errno, ECANCELED);

	/* the same number of allocations should be possible. */
	while (pmemobj_alloc(pop, NULL, node_size, 1, NULL, NULL) == 0)
		allocs--;
	UT_ASSERT(allocs <= 0);

	POBJ_FOREACH_SAFE(pop, oid, next)
		pmemobj_free(&oid);

	root.oid = pmemobj_root_construct(pop, sizeof(struct root),
			NULL, NULL);
	UT_ASSERT(!TOID_IS_NULL(root));

	errno = 0;
	if (big) {
		node_big.oid = pmemobj_list_insert_new(pop, next_off,
				&D_RW(root)->list_big, OID_NULL, 0, node_size,
				1, node_constr_cancel, NULL);
		UT_ASSERT(TOID_IS_NULL(node_big));
	} else {
		node.oid = pmemobj_list_insert_new(pop, next_off,
				&D_RW(root)->list, OID_NULL, 0, node_size,
				1, node_constr_cancel, NULL);
		UT_ASSERT(TOID_IS_NULL(node));
	}
	UT_ASSERTeq(errno, ECANCELED);

	pmemobj_alloc(pop, &oid, sizeof(struct foo), 1,
		vg_test_save_ptr, NULL);
	UT_ASSERTne(Canceled_ptr, NULL);

	/* this should generate a valgrind memcheck warning */
	Canceled_ptr->bar = 5;
	pmemobj_persist(pop, &Canceled_ptr->bar, sizeof(Canceled_ptr->bar));

	/*
	 * Allocate and cancel a huge object. It should return back to the
	 * heap and it should be possible to allocate it again.
	 */
	Canceled_ptr = NULL;
	ret = pmemobj_alloc(pop, &oid, sizeof(struct foo) + (1 << 22), 1,
		vg_test_save_ptr, NULL);
	UT_ASSERTne(Canceled_ptr, NULL);
	void *first_ptr = Canceled_ptr;
	Canceled_ptr = NULL;

	ret = pmemobj_alloc(pop, &oid, sizeof(struct foo) + (1 << 22), 1,
		vg_test_save_ptr, NULL);

	UT_ASSERTeq(first_ptr, Canceled_ptr);

	pmemobj_close(pop);

	DONE(NULL);
}
