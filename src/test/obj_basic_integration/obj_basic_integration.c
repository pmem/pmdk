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
 * obj_basic_integration.c -- Basic integration tests
 *
 */

#include <stddef.h>

#include "unittest.h"

#define TEST_STR "abcdefgh"
#define TEST_STR_LEN 8
#define TEST_VALUE 5

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

static int
dummy_node_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct dummy_node *n = (struct dummy_node *)ptr;
	int *test_val = (int *)arg;
	n->value = *test_val;
	pmemobj_persist(pop, &n->value, sizeof(n->value));

	return 0;
}

static void
test_alloc_api(PMEMobjpool *pop)
{
	TOID(struct dummy_node) node_zeroed;
	TOID(struct dummy_node_c) node_constructed;

	POBJ_ZNEW(pop, &node_zeroed, struct dummy_node);

	UT_ASSERT_rt(OID_INSTANCEOF(node_zeroed.oid, struct dummy_node));

	int *test_val = (int *)MALLOC(sizeof(*test_val));
	*test_val = TEST_VALUE;
	POBJ_NEW(pop, &node_constructed, struct dummy_node_c,
			dummy_node_constructor, test_val);

	FREE(test_val);

	TOID(struct dummy_node) iter;

	POBJ_FOREACH_TYPE(pop, iter) {
		UT_ASSERTeq(D_RO(iter)->value, 0);
	}

	TOID(struct dummy_node_c) iter_c;
	POBJ_FOREACH_TYPE(pop, iter_c) {
		UT_ASSERTeq(D_RO(iter_c)->value, TEST_VALUE);
	}

	PMEMoid oid_iter;
	int nodes_count = 0;
	POBJ_FOREACH(pop, oid_iter) {
		nodes_count++;
	}
	UT_ASSERTne(nodes_count, 0);

	POBJ_FREE(&node_zeroed);
	POBJ_FREE(&node_constructed);

	nodes_count = 0;
	POBJ_FOREACH(pop, oid_iter) {
		nodes_count++;
	}
	UT_ASSERTeq(nodes_count, 0);

	int val = 10;
	POBJ_ALLOC(pop, &node_constructed, struct dummy_node_c,
			sizeof(struct dummy_node_c),
			dummy_node_constructor, &val);

	POBJ_REALLOC(pop, &node_constructed, struct dummy_node_c,
			sizeof(struct dummy_node_c) + 1000);

	UT_ASSERTeq(pmemobj_type_num(node_constructed.oid),
			TOID_TYPE_NUM(struct dummy_node_c));

	POBJ_ZREALLOC(pop, &node_constructed, struct dummy_node_c,
			sizeof(struct dummy_node_c) + 2000);

	UT_ASSERTeq(pmemobj_type_num(node_constructed.oid),
			TOID_TYPE_NUM(struct dummy_node_c));

	POBJ_FREE(&node_constructed);

	POBJ_ZALLOC(pop, &node_zeroed, struct dummy_node,
			sizeof(struct dummy_node));

	POBJ_FREE(&node_zeroed);

	int err = 0;

	err = pmemobj_alloc(pop, NULL, SIZE_MAX, 0, NULL, NULL);
	UT_ASSERTeq(err, -1);
	UT_ASSERTeq(errno, ENOMEM);

	err = pmemobj_zalloc(pop, NULL, SIZE_MAX, 0);
	UT_ASSERTeq(err, -1);
	UT_ASSERTeq(errno, ENOMEM);

	err = pmemobj_alloc(pop, NULL, PMEMOBJ_MAX_ALLOC_SIZE + 1, 0, NULL,
		NULL);
	UT_ASSERTeq(err, -1);
	UT_ASSERTeq(errno, ENOMEM);

	err = pmemobj_zalloc(pop, NULL, PMEMOBJ_MAX_ALLOC_SIZE + 1, 0);
	UT_ASSERTeq(err, -1);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
test_realloc_api(PMEMobjpool *pop)
{
	PMEMoid oid = OID_NULL;
	int ret;

	ret = pmemobj_alloc(pop, &oid, 128, 0, NULL, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("alloc: %u, size: %zu", 128,
			pmemobj_alloc_usable_size(oid));

	/* grow */
	ret = pmemobj_realloc(pop, &oid, 655360, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("realloc: %u => %u, size: %zu", 128, 655360,
			pmemobj_alloc_usable_size(oid));

	/* shrink */
	ret = pmemobj_realloc(pop, &oid, 1, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("realloc: %u => %u, size: %zu", 655360, 1,
			pmemobj_alloc_usable_size(oid));

	/* free */
	ret = pmemobj_realloc(pop, &oid, 0, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(OID_IS_NULL(oid));
	UT_OUT("free");

	/* alloc */
	ret = pmemobj_realloc(pop, &oid, 777, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("realloc: %u => %u, size: %zu", 0, 777,
			pmemobj_alloc_usable_size(oid));

	/* shrink */
	ret = pmemobj_realloc(pop, &oid, 1, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("realloc: %u => %u, size: %zu", 777, 1,
			pmemobj_alloc_usable_size(oid));

	pmemobj_free(&oid);
	UT_ASSERT(OID_IS_NULL(oid));
	UT_OUT("free");

	/* alloc */
	ret = pmemobj_realloc(pop, &oid, 1, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("realloc: %u => %u, size: %zu", 0, 1,
			pmemobj_alloc_usable_size(oid));

	/* do nothing */
	ret = pmemobj_realloc(pop, &oid, 1, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));
	UT_OUT("realloc: %u => %u, size: %zu", 1, 1,
			pmemobj_alloc_usable_size(oid));

	pmemobj_free(&oid);
	UT_ASSERT(OID_IS_NULL(oid));
	UT_OUT("free");

	/* do nothing */
	ret = pmemobj_realloc(pop, &oid, 0, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(OID_IS_NULL(oid));

	/* alloc */
	ret = pmemobj_realloc(pop, &oid, 1, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(!OID_IS_NULL(oid));

	/* grow beyond reasonable size */
	ret = pmemobj_realloc(pop, &oid, SIZE_MAX, 0);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(errno, ENOMEM);

	ret = pmemobj_realloc(pop, &oid, PMEMOBJ_MAX_ALLOC_SIZE + 1, 0);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(errno, ENOMEM);

	pmemobj_free(&oid);
	UT_ASSERT(OID_IS_NULL(oid));
}

static void
test_list_api(PMEMobjpool *pop)
{
	TOID(struct dummy_root) root;
	root = POBJ_ROOT(pop, struct dummy_root);
	int nodes_count = 0;

	UT_ASSERTeq(pmemobj_type_num(root.oid), POBJ_ROOT_TYPE_NUM);
	UT_COMPILE_ERROR_ON(TOID_TYPE_NUM_OF(root) != POBJ_ROOT_TYPE_NUM);

	TOID(struct dummy_node) first;
	TOID(struct dummy_node) iter;

	POBJ_LIST_FOREACH_REVERSE(iter, &D_RO(root)->dummies, plist) {
		UT_OUT("POBJ_LIST_FOREACH_REVERSE: dummy_node %d",
					D_RO(iter)->value);
		nodes_count++;
	}

	UT_ASSERTeq(nodes_count, 0);

	int test_val = TEST_VALUE;
	PMEMoid ret;

	/* should fail */
	ret = POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->dummies, plist,
			SIZE_MAX, dummy_node_constructor,
			&test_val);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERT(OID_IS_NULL(ret));

	errno = 0;
	ret = POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->dummies, plist,
			PMEMOBJ_MAX_ALLOC_SIZE + 1, dummy_node_constructor,
			&test_val);
	UT_ASSERTeq(errno, ENOMEM);
	UT_ASSERT(OID_IS_NULL(ret));

	POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->dummies, plist,
			sizeof(struct dummy_node), dummy_node_constructor,
			&test_val);
	test_val++;
	POBJ_LIST_INSERT_NEW_TAIL(pop, &D_RW(root)->dummies, plist,
			sizeof(struct dummy_node), dummy_node_constructor,
			&test_val);

	TOID(struct dummy_node) node;
	POBJ_ZNEW(pop, &node, struct dummy_node);

	POBJ_LIST_INSERT_HEAD(pop, &D_RW(root)->dummies, node, plist);

	nodes_count = 0;

	POBJ_LIST_FOREACH(iter, &D_RO(root)->dummies, plist) {
		UT_OUT("POBJ_LIST_FOREACH: dummy_node %d", D_RO(iter)->value);
		nodes_count++;
	}

	UT_ASSERTeq(nodes_count, 3);

	/* now do the same, but w/o using FOREACH macro */
	nodes_count = 0;
	first = POBJ_LIST_FIRST(&D_RO(root)->dummies);
	iter = first;
	do {
		UT_OUT("POBJ_LIST_NEXT: dummy_node %d", D_RO(iter)->value);
		nodes_count++;
		iter = POBJ_LIST_NEXT(iter, plist);
	} while (!TOID_EQUALS(iter, first));
	UT_ASSERTeq(nodes_count, 3);

	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(root)->dummies,
		&D_RW(root)->moved, node, plist, plist_m);

	UT_ASSERTeq(POBJ_LIST_EMPTY(&D_RW(root)->moved), 0);

	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(root)->moved,
		&D_RW(root)->dummies, node, plist_m, plist);

	POBJ_LIST_MOVE_ELEMENT_TAIL(pop, &D_RW(root)->dummies,
		&D_RW(root)->moved, node, plist, plist_m);

	UT_ASSERTeq(POBJ_LIST_EMPTY(&D_RW(root)->moved), 0);

	POBJ_LIST_MOVE_ELEMENT_TAIL(pop, &D_RW(root)->moved,
		&D_RW(root)->dummies, node, plist_m, plist);

	POBJ_LIST_REMOVE(pop, &D_RW(root)->dummies, node, plist);
	POBJ_LIST_INSERT_TAIL(pop, &D_RW(root)->dummies, node, plist);
	POBJ_LIST_REMOVE_FREE(pop, &D_RW(root)->dummies, node, plist);

	nodes_count = 0;
	POBJ_LIST_FOREACH_REVERSE(iter, &D_RO(root)->dummies, plist) {
		UT_OUT("POBJ_LIST_FOREACH_REVERSE: dummy_node %d",
					D_RO(iter)->value);
		nodes_count++;
	}
	UT_ASSERTeq(nodes_count, 2);

	/* now do the same, but w/o using FOREACH macro */
	nodes_count = 0;
	first = POBJ_LIST_FIRST(&D_RO(root)->dummies);
	iter = first;
	do {
		UT_OUT("POBJ_LIST_PREV: dummy_node %d", D_RO(iter)->value);
		nodes_count++;
		iter = POBJ_LIST_PREV(iter, plist);
	} while (!TOID_EQUALS(iter, first));
	UT_ASSERTeq(nodes_count, 2);

	test_val++;
	POBJ_LIST_INSERT_NEW_AFTER(pop, &D_RW(root)->dummies,
		POBJ_LIST_FIRST(&D_RO(root)->dummies), plist,
		sizeof(struct dummy_node), dummy_node_constructor,
		&test_val);

	test_val++;
	POBJ_LIST_INSERT_NEW_BEFORE(pop, &D_RW(root)->dummies,
		POBJ_LIST_LAST(&D_RO(root)->dummies, plist), plist,
		sizeof(struct dummy_node), dummy_node_constructor,
		&test_val);

	nodes_count = 0;
	POBJ_LIST_FOREACH_REVERSE(iter, &D_RO(root)->dummies, plist) {
		UT_OUT("POBJ_LIST_FOREACH_REVERSE: dummy_node %d",
					D_RO(iter)->value);
		nodes_count++;
	}
	UT_ASSERTeq(nodes_count, 4);

	/* now do the same, but w/o using FOREACH macro */
	nodes_count = 0;
	first = POBJ_LIST_LAST(&D_RO(root)->dummies, plist);
	iter = first;
	do {
		UT_OUT("POBJ_LIST_PREV: dummy_node %d", D_RO(iter)->value);
		nodes_count++;
		iter = POBJ_LIST_PREV(iter, plist);
	} while (!TOID_EQUALS(iter, first));
	UT_ASSERTeq(nodes_count, 4);
}

static void
test_tx_api(PMEMobjpool *pop)
{
	TOID(struct dummy_root) root;
	TOID_ASSIGN(root, pmemobj_root(pop, sizeof(struct dummy_root)));

	int *vstate = NULL; /* volatile state */

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		vstate = (int *)MALLOC(sizeof(*vstate));
		*vstate = TEST_VALUE;
		TX_ADD(root);
		D_RW(root)->value = *vstate;
		TOID_ASSIGN(D_RW(root)->node, OID_NULL);
	} TX_FINALLY {
		FREE(vstate);
		vstate = NULL;
	} TX_END

	UT_ASSERTeq(vstate, NULL);
	UT_ASSERTeq(D_RW(root)->value, TEST_VALUE);

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		TX_ADD(root);
		D_RW(root)->node = TX_ALLOC(struct dummy_node, SIZE_MAX);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END

	errno = 0;
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		D_RW(root)->node = TX_ZALLOC(struct dummy_node, SIZE_MAX);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END

	errno = 0;
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		D_RW(root)->node = TX_XALLOC(struct dummy_node, SIZE_MAX,
				POBJ_XALLOC_ZERO);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END

	errno = 0;
	TX_BEGIN_LOCK(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		D_RW(root)->node = TX_ALLOC(struct dummy_node,
			PMEMOBJ_MAX_ALLOC_SIZE + 1);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END

	errno = 0;
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		D_RW(root)->node = TX_ZALLOC(struct dummy_node,
			PMEMOBJ_MAX_ALLOC_SIZE + 1);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END

	errno = 0;
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		TX_ADD(root);
		D_RW(root)->node = TX_ZNEW(struct dummy_node);
		TX_REALLOC(D_RO(root)->node, SIZE_MAX);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));

	errno = 0;
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		TX_ADD(root);
		D_RW(root)->node = TX_ZNEW(struct dummy_node);
		TX_REALLOC(D_RO(root)->node, PMEMOBJ_MAX_ALLOC_SIZE + 1);
		UT_ASSERT(0); /* should not get to this point */
	} TX_ONABORT {
		UT_ASSERTeq(errno, ENOMEM);
	} TX_END
	UT_ASSERT(TOID_IS_NULL(D_RO(root)->node));

	errno = 0;
	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		TX_ADD(root);
		D_RW(root)->node = TX_ZNEW(struct dummy_node);
		TX_MEMSET(D_RW(D_RW(root)->node)->teststr, 'a', TEST_STR_LEN);
		TX_MEMCPY(D_RW(D_RW(root)->node)->teststr, TEST_STR,
			TEST_STR_LEN);
		TX_SET(D_RW(root)->node, value, TEST_VALUE);
	} TX_END
	UT_ASSERTeq(D_RW(D_RW(root)->node)->value, TEST_VALUE);
	UT_ASSERT(strncmp(D_RW(D_RW(root)->node)->teststr, TEST_STR,
		TEST_STR_LEN) == 0);

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &D_RW(root)->lock) {
		TX_ADD(root);
		UT_ASSERT(!TOID_IS_NULL(D_RW(root)->node));
		TX_FREE(D_RW(root)->node);
		D_RW(root)->node = TOID_NULL(struct dummy_node);
		TOID_ASSIGN(D_RW(root)->node, OID_NULL);
	} TX_END

	errno = 0;
	TX_BEGIN(pop) {
		TX_BEGIN(NULL) {
		} TX_ONCOMMIT {
			UT_ASSERT(0);
		} TX_END
		UT_ASSERT(errno == EFAULT);
	} TX_END

	errno = 0;
	TX_BEGIN(pop) {
		TX_BEGIN((PMEMobjpool *)(uintptr_t)7) {
		} TX_ONCOMMIT {
			UT_ASSERT(0);
		} TX_END
		UT_ASSERT(errno == EINVAL);
	} TX_END

	UT_OUT("%s", pmemobj_errormsg());
	TX_BEGIN(pop) {
		pmemobj_tx_abort(ECANCELED);
	} TX_END
	UT_OUT("%s", pmemobj_errormsg());
}

static void
test_offsetof(void)
{
	TOID(struct dummy_root) r;
	TOID(struct dummy_node) n;

	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(r, value) !=
				offsetof(struct dummy_root, value));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(r, lock) !=
				offsetof(struct dummy_root, lock));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(r, node) !=
				offsetof(struct dummy_root, node));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(r, dummies) !=
				offsetof(struct dummy_root, dummies));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(r, moved) !=
				offsetof(struct dummy_root, moved));

	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(n, value) !=
				offsetof(struct dummy_node, value));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(n, teststr) !=
				offsetof(struct dummy_node, teststr));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(n, plist) !=
				offsetof(struct dummy_node, plist));
	UT_COMPILE_ERROR_ON(TOID_OFFSETOF(n, plist_m) !=
				offsetof(struct dummy_node, plist_m));
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_basic_integration");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(basic) != 2);

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(basic),
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	test_alloc_api(pop);
	test_realloc_api(pop);
	test_list_api(pop);
	test_tx_api(pop);
	test_offsetof();

	pmemobj_close(pop);

	if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(basic))) == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	/* second open should fail, checks file locking */
	if ((pmemobj_open(path, POBJ_LAYOUT_NAME(basic))) != NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	pmemobj_close(pop);

	int result = pmemobj_check(path, POBJ_LAYOUT_NAME(basic));
	if (result < 0)
		UT_OUT("!%s: pmemobj_check", path);
	else if (result == 0)
		UT_OUT("%s: pmemobj_check: not consistent", path);

	DONE(NULL);
}
