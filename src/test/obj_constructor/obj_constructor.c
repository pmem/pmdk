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
 * obj_constructor.c -- tests for constructor
 */

#include <stddef.h>

#include "unittest.h"

/*
 * Command line toggle indicating use of a bigger node structure for querying
 * pool size expressed in a number of possible allocations. A small node
 * structure results in a great number of allocations impossible to replicate
 * in assumed timeout. It is required by unit tests using remote replication to
 * pass on Travis.
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
