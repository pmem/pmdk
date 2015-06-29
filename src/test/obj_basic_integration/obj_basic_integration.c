/*
 * Copyright (c) 2015, Intel Corporation
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
 * obj_basic_integration.c -- Basic integration tests
 *
 */

#include <stddef.h>

#include "unittest.h"

#define	TEST_STR "abcdefgh"
#define	TEST_STR_LEN 8
#define	TEST_VALUE 5
#define	TEST_ALLOC_SIZE 8000

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(basic);
POBJ_LAYOUT_ROOT(basic, struct dummy_root);
POBJ_LAYOUT_TOID(basic, struct dummy_node);
POBJ_LAYOUT_TOID(basic, struct dummy_node_c);
POBJ_LAYOUT_END(basic);

struct dummy_node {
	int value;
	char teststr[TEST_STR_LEN];
	POBJ_LIST_ENTRY(struct dummy_node) plist;
	POBJ_LIST_ENTRY(struct dummy_node) plist_m;
};

struct dummy_node_c {
	int value;
	char teststr[TEST_STR_LEN];
	POBJ_LIST_ENTRY(struct dummy_node) plist;
	POBJ_LIST_ENTRY(struct dummy_node) plist_m;
};

struct dummy_root {
	int value;
	PMEMmutex lock;
	TOID(struct dummy_node) node;
	POBJ_LIST_HEAD(dummy_list, struct dummy_node) dummies;
	POBJ_LIST_HEAD(moved_list, struct dummy_node) moved;
};

void
dummy_node_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct dummy_node *n = ptr;
	int *test_val = arg;
	n->value = *test_val;
}

void
test_alloc_api(PMEMobjpool *pop)
{
	TOID(struct dummy_node) node_zeroed;
	TOID(struct dummy_node_c) node_constructed;

	POBJ_ZNEW(pop, &node_zeroed, struct dummy_node);

	ASSERT(OID_INSTANCEOF(node_zeroed.oid, struct dummy_node));

	int *test_val = MALLOC(sizeof (*test_val));
	*test_val = TEST_VALUE;
	POBJ_NEW(pop, &node_constructed, struct dummy_node_c,
			dummy_node_constructor, test_val);

	FREE(test_val);

	TOID(struct dummy_node) iter;

	POBJ_FOREACH_TYPE(pop, iter) {
		ASSERTeq(D_RO(iter)->value, 0);
	}

	TOID(struct dummy_node_c) iter_c;
	POBJ_FOREACH_TYPE(pop, iter_c) {
		ASSERTeq(D_RO(iter_c)->value, TEST_VALUE);
	}

	PMEMoid oid_iter;
	int type_iter;
	POBJ_FOREACH(pop, oid_iter, type_iter) {
		ASSERT(type_iter == TOID_TYPE_NUM(struct dummy_node) ||
			type_iter == TOID_TYPE_NUM(struct dummy_node_c));
	}

	POBJ_FREE(&node_zeroed);
	POBJ_FREE(&node_constructed);

	int nodes_count = 0;
	POBJ_FOREACH(pop, oid_iter, type_iter) {
		nodes_count++;
	}
	ASSERTeq(nodes_count, 0);
}

void
test_list_api(PMEMobjpool *pop)
{
	TOID(struct dummy_root) root;
	root = POBJ_ROOT(pop, struct dummy_root);
	int nodes_count = 0;

	TOID(struct dummy_node) iter;
	POBJ_LIST_FOREACH_REVERSE(iter, &D_RO(root)->dummies, plist) {
		OUT("dummy_node %d", D_RO(iter)->value);
		nodes_count++;
	}

	ASSERTeq(nodes_count, 0);

	int *test_val = MALLOC(sizeof (*test_val));
	*test_val = TEST_VALUE;

	POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->dummies, plist,
			sizeof (struct dummy_node), dummy_node_constructor,
			test_val);
	POBJ_LIST_INSERT_NEW_TAIL(pop, &D_RW(root)->dummies, plist,
			sizeof (struct dummy_node), dummy_node_constructor,
			test_val);

	FREE(test_val);

	TOID(struct dummy_node) node;
	POBJ_ZNEW(pop, &node, struct dummy_node);

	POBJ_LIST_INSERT_HEAD(pop, &D_RW(root)->dummies, node, plist);

	nodes_count = 0;

	POBJ_LIST_FOREACH(iter, &D_RO(root)->dummies, plist) {
		OUT("dummy_node %d", D_RO(iter)->value);
		nodes_count++;
	}

	ASSERTeq(nodes_count, 3);

	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(root)->dummies,
		&D_RW(root)->moved, node, plist, plist_m);

	ASSERTeq(POBJ_LIST_EMPTY(&D_RW(root)->moved), 0);

	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(root)->moved,
		&D_RW(root)->dummies, node, plist_m, plist);

	POBJ_LIST_REMOVE(pop, &D_RW(root)->dummies, node, plist);
	POBJ_LIST_INSERT_TAIL(pop, &D_RW(root)->dummies, node, plist);
	POBJ_LIST_REMOVE_FREE(pop, &D_RW(root)->dummies, node, plist);

	nodes_count = 0;
	POBJ_LIST_FOREACH_REVERSE(iter, &D_RO(root)->dummies, plist) {
		OUT("reverse dummy_node %d", D_RO(iter)->value);
		nodes_count++;
	}
	ASSERTeq(nodes_count, 2);
}

void
test_tx_api(PMEMobjpool *pop)
{
	TOID(struct dummy_root) root;
	TOID_ASSIGN(root, pmemobj_root(pop, sizeof (struct dummy_root)));

	int *vstate = NULL; /* volatile state */

	TX_BEGIN_LOCK(pop, TX_LOCK_MUTEX, &D_RW(root)->lock) {
		vstate = MALLOC(sizeof (*vstate));
		*vstate = TEST_VALUE;
		TX_ADD(root);
		D_RW(root)->value = *vstate;
		D_RW(root)->node = TX_ZNEW(struct dummy_node);
		TX_MEMSET(D_RW(D_RW(root)->node)->teststr, 'a', TEST_STR_LEN);
		TX_MEMCPY(D_RW(D_RW(root)->node)->teststr, TEST_STR,
			TEST_STR_LEN);
		TX_SET(D_RW(root)->node, value, TEST_VALUE);
	} TX_FINALLY {
		FREE(vstate);
		vstate = NULL;
	} TX_END

	ASSERTeq(vstate, NULL);
	ASSERTeq(D_RW(root)->value, TEST_VALUE);

	TX_BEGIN_LOCK(pop, TX_LOCK_MUTEX, &D_RW(root)->lock) {
		ASSERT(!TOID_IS_NULL(D_RW(root)->node));
		TX_FREE(D_RW(root)->node);
		D_RW(root)->node = TOID_NULL(struct dummy_node);
		TOID_ASSIGN(D_RW(root)->node, OID_NULL);
	} TX_END
}

void
test_open_close(PMEMobjpool *pop, const char *path)
{
	PMEMoid oid;
	pmemobj_alloc(pop, &oid, TEST_ALLOC_SIZE, 1, NULL, NULL);

	pmemobj_close(pop);

	ASSERT(pmemobj_check(path, POBJ_LAYOUT_NAME(basic)) == 1);

	ASSERT((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(basic))) != NULL);

	pmemobj_free(&oid);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_basic_integration");

	if (argc != 2)
		FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(basic),
			PMEMOBJ_MIN_POOL, S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	test_alloc_api(pop);
	test_list_api(pop);
	test_tx_api(pop);
	test_open_close(pop, path);

	pmemobj_close(pop);

	DONE(NULL);
}
