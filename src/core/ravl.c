// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2024, Intel Corporation */

/*
 * ravl.c -- implementation of a RAVL tree
 * https://sidsen.azurewebsites.net//papers/ravl-trees-journal.pdf
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "out.h"
#include "core_assert.h"
#include "ravl.h"
#include "alloc.h"

#define RAVL_DEFAULT_DATA_SIZE (sizeof(void *))

enum ravl_slot_type {
	RAVL_LEFT,
	RAVL_RIGHT,

	MAX_SLOTS,

	RAVL_ROOT
};

struct ravl_node {
	struct ravl_node *parent;
	struct ravl_node *slots[MAX_SLOTS];
	int32_t rank; /* cannot be greater than height of the subtree */
	int32_t pointer_based;
	char data[];
};

struct ravl {
	struct ravl_node *root;
	ravl_compare *compare;
	size_t data_size;
};

/*
 * ravl_new -- creates a new ravl tree instance
 */
struct ravl *
ravl_new_sized(ravl_compare *compare, size_t data_size)
{
	struct ravl *r = Malloc(sizeof(*r));
	if (r == NULL) {
		ERR_W_ERRNO("Malloc");
		return r;
	}

	r->compare = compare;
	r->root = NULL;
	r->data_size = data_size;

	return r;
}

/*
 * ravl_new -- creates a new tree that stores data pointers
 */
struct ravl *
ravl_new(ravl_compare *compare)
{
	return ravl_new_sized(compare, RAVL_DEFAULT_DATA_SIZE);
}

/*
 * ravl_clear_node -- (internal) recursively clears the given subtree,
 *	calls callback in an in-order fashion. Optionally frees the given node.
 */
static void
ravl_foreach_node(struct ravl_node *n, ravl_cb cb, void *arg, int free_node)
{
	if (n == NULL)
		return;

	ravl_foreach_node(n->slots[RAVL_LEFT], cb, arg, free_node);
	if (cb)
		cb((void *)n->data, arg);
	ravl_foreach_node(n->slots[RAVL_RIGHT], cb, arg, free_node);

	if (free_node)
		Free(n);
}

/*
 * ravl_clear -- clears the entire tree, starting from the root
 */
void
ravl_clear(struct ravl *ravl)
{
	ravl_foreach_node(ravl->root, NULL, NULL, 1);
	ravl->root = NULL;
}

/*
 * ravl_delete_cb -- clears and deletes the given ravl instance, calls callback
 */
void
ravl_delete_cb(struct ravl *ravl, ravl_cb cb, void *arg)
{
	ravl_foreach_node(ravl->root, cb, arg, 1);
	Free(ravl);
}

/*
 * ravl_delete -- clears and deletes the given ravl instance
 */
void
ravl_delete(struct ravl *ravl)
{
	ravl_delete_cb(ravl, NULL, NULL);
}

/*
 * ravl_foreach -- traverses the entire tree, calling callback for every node
 */
void
ravl_foreach(struct ravl *ravl, ravl_cb cb, void *arg)
{
	ravl_foreach_node(ravl->root, cb, arg, 0);
}

/*
 * ravl_empty -- checks whether the given tree is empty
 */
int
ravl_empty(struct ravl *ravl)
{
	return ravl->root == NULL;
}

/*
 * ravl_node_insert_constructor -- node data constructor for ravl_insert
 */
static void
ravl_node_insert_constructor(void *data, size_t data_size, const void *arg)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(data_size);

	/* copy only the 'arg' pointer */
	memcpy(data, &arg, sizeof(arg));
}

/*
 * ravl_node_copy_constructor -- node data constructor for ravl_emplace_copy
 */
static void
ravl_node_copy_constructor(void *data, size_t data_size, const void *arg)
{
	memcpy(data, arg, data_size);
}

/*
 * ravl_new_node -- (internal) allocates and initializes a new node
 */
static struct ravl_node *
ravl_new_node(struct ravl *ravl, ravl_constr constr, const void *arg)
{
	struct ravl_node *n = Malloc(sizeof(*n) + ravl->data_size);
	if (n == NULL) {
		ERR_W_ERRNO("Malloc");
		return n;
	}

	n->parent = NULL;
	n->slots[RAVL_LEFT] = NULL;
	n->slots[RAVL_RIGHT] = NULL;
	n->rank = 0;
	n->pointer_based = constr == ravl_node_insert_constructor;
	constr(n->data, ravl->data_size, arg);

	return n;
}

/*
 * ravl_slot_opposite -- (internal) returns the opposite slot type, cannot be
 *	called for root type
 */
static enum ravl_slot_type
ravl_slot_opposite(enum ravl_slot_type t)
{
	ASSERTne(t, RAVL_ROOT);

	return t == RAVL_LEFT ? RAVL_RIGHT : RAVL_LEFT;
}

/*
 * ravl_node_slot_type -- (internal) returns the type of the given node:
 *	left child, right child or root
 */
static enum ravl_slot_type
ravl_node_slot_type(struct ravl_node *n)
{
	if (n->parent == NULL)
		return RAVL_ROOT;

	return n->parent->slots[RAVL_LEFT] == n ? RAVL_LEFT : RAVL_RIGHT;
}

/*
 * ravl_node_sibling -- (internal) returns the sibling of the given node,
 *	NULL if the node is root (has no parent)
 */
static struct ravl_node *
ravl_node_sibling(struct ravl_node *n)
{
	enum ravl_slot_type t = ravl_node_slot_type(n);
	if (t == RAVL_ROOT)
		return NULL;

	return n->parent->slots[t == RAVL_LEFT ? RAVL_RIGHT : RAVL_LEFT];
}

/*
 * ravl_node_ref -- (internal) returns the pointer to the memory location in
 *	which the given node resides
 */
static struct ravl_node **
ravl_node_ref(struct ravl *ravl, struct ravl_node *n)
{
	enum ravl_slot_type t = ravl_node_slot_type(n);

	return t == RAVL_ROOT ? &ravl->root : &n->parent->slots[t];
}

/*
 * ravl_rotate -- (internal) performs a rotation around a given node
 *
 * The node n swaps place with its parent. If n is right child, parent becomes
 * the left child of n, otherwise parent becomes right child of n.
 */
static void
ravl_rotate(struct ravl *ravl, struct ravl_node *n)
{
	ASSERTne(n->parent, NULL);
	struct ravl_node *p = n->parent;
	struct ravl_node **pref = ravl_node_ref(ravl, p);

	enum ravl_slot_type t = ravl_node_slot_type(n);
	enum ravl_slot_type t_opposite = ravl_slot_opposite(t);

	n->parent = p->parent;
	p->parent = n;
	*pref = n;

	if ((p->slots[t] = n->slots[t_opposite]) != NULL)
		p->slots[t]->parent = p;
	n->slots[t_opposite] = p;
}

/*
 * ravl_node_rank -- (internal) returns the rank of the node
 *
 * For the purpose of balancing, NULL nodes have rank -1.
 */
static int
ravl_node_rank(struct ravl_node *n)
{
	return n == NULL ? -1 : n->rank;
}

/*
 * ravl_node_rank_difference_parent -- (internal) returns the rank different
 *	between parent node p and its child n
 *
 * Every rank difference must be positive.
 *
 * Either of these can be NULL.
 */
static int
ravl_node_rank_difference_parent(struct ravl_node *p, struct ravl_node *n)
{
	return ravl_node_rank(p) - ravl_node_rank(n);
}

/*
 * ravl_node_rank_differenced - (internal) returns the rank difference between
 *	parent and its child
 *
 * Can be used to check if a given node is an i-child.
 */
static int
ravl_node_rank_difference(struct ravl_node *n)
{
	return ravl_node_rank_difference_parent(n->parent, n);
}

/*
 * ravl_node_is_i_j -- (internal) checks if a given node is strictly i,j-node
 */
static int
ravl_node_is_i_j(struct ravl_node *n, int i, int j)
{
	return (ravl_node_rank_difference_parent(n, n->slots[RAVL_LEFT]) == i &&
		ravl_node_rank_difference_parent(n, n->slots[RAVL_RIGHT]) == j);
}

/*
 * ravl_node_is -- (internal) checks if a given node is i,j-node or j,i-node
 */
static int
ravl_node_is(struct ravl_node *n, int i, int j)
{
	return ravl_node_is_i_j(n, i, j) || ravl_node_is_i_j(n, j, i);
}

/*
 * ravl_node_promote -- promotes a given node by increasing its rank
 */
static void
ravl_node_promote(struct ravl_node *n)
{
	n->rank += 1;
}

/*
 * ravl_node_promote -- demotes a given node by increasing its rank
 */
static void
ravl_node_demote(struct ravl_node *n)
{
	ASSERT(n->rank > 0);
	n->rank -= 1;
}

/*
 * ravl_balance -- balances the tree after insert
 *
 * This function must restore the invariant that every rank
 * difference is positive.
 */
static void
ravl_balance(struct ravl *ravl, struct ravl_node *n)
{
	/* walk up the tree, promoting nodes */
	while (n->parent && ravl_node_is(n->parent, 0, 1)) {
		ravl_node_promote(n->parent);
		n = n->parent;
	}

	/*
	 * Either the rank rule holds or n is a 0-child whose sibling is an
	 * i-child with i > 1.
	 */
	struct ravl_node *s = ravl_node_sibling(n);
	if (!(ravl_node_rank_difference(n) == 0 &&
	    ravl_node_rank_difference_parent(n->parent, s) > 1))
		return;

	struct ravl_node *y = n->parent;
	/* if n is a left child, let z be n's right child and vice versa */
	enum ravl_slot_type t = ravl_slot_opposite(ravl_node_slot_type(n));
	struct ravl_node *z = n->slots[t];

	if (z == NULL || ravl_node_rank_difference(z) == 2) {
		ravl_rotate(ravl, n);
		ravl_node_demote(y);
	} else if (ravl_node_rank_difference(z) == 1) {
		ravl_rotate(ravl, z);
		ravl_rotate(ravl, z);
		ravl_node_promote(z);
		ravl_node_demote(n);
		ravl_node_demote(y);
	}
}

/*
 * ravl_insert -- insert data into the tree
 */
int
ravl_insert(struct ravl *ravl, const void *data)
{
	return ravl_emplace(ravl, ravl_node_insert_constructor, data);
}

/*
 * ravl_insert -- copy construct data inside of a new tree node
 */
int
ravl_emplace_copy(struct ravl *ravl, const void *data)
{
	return ravl_emplace(ravl, ravl_node_copy_constructor, data);
}

/*
 * ravl_emplace -- construct data inside of a new tree node
 */
int
ravl_emplace(struct ravl *ravl, ravl_constr constr, const void *arg)
{
	LOG(6, NULL);

	struct ravl_node *n = ravl_new_node(ravl, constr, arg);
	if (n == NULL)
		return -1;

	/* walk down the tree and insert the new node into a missing slot */
	struct ravl_node **dstp = &ravl->root;
	struct ravl_node *dst = NULL;
	while (*dstp != NULL) {
		dst = (*dstp);
		int cmp_result = ravl->compare(ravl_data(n), ravl_data(dst));
		if (cmp_result == 0)
			goto error_duplicate;

		dstp = &dst->slots[cmp_result > 0];
	}
	n->parent = dst;
	*dstp = n;

	ravl_balance(ravl, n);

	return 0;

error_duplicate:
	errno = EEXIST;
	Free(n);
	return -1;
}

/*
 * ravl_node_type_most -- (internal) returns left-most or right-most node in
 *	the subtree
 */
static struct ravl_node *
ravl_node_type_most(struct ravl_node *n, enum ravl_slot_type t)
{
	while (n->slots[t] != NULL)
		n = n->slots[t];

	return n;
}

/*
 * ravl_node_cessor -- (internal) returns the successor or predecessor of the
 *	node
 */
static struct ravl_node *
ravl_node_cessor(struct ravl_node *n, enum ravl_slot_type t)
{
	/*
	 * If t child is present, we are looking for t-opposite-most node
	 * in t child subtree
	 */
	if (n->slots[t])
		return ravl_node_type_most(n->slots[t], ravl_slot_opposite(t));

	/* otherwise get the first parent on the t path */
	while (n->parent != NULL && n == n->parent->slots[t])
		n = n->parent;

	return n->parent;
}

/*
 * ravl_node_successor -- (internal) returns node's successor
 *
 * It's the first node larger than n.
 */
static struct ravl_node *
ravl_node_successor(struct ravl_node *n)
{
	return ravl_node_cessor(n, RAVL_RIGHT);
}

/*
 * ravl_node_successor -- (internal) returns node's successor
 *
 * It's the first node smaller than n.
 */
static struct ravl_node *
ravl_node_predecessor(struct ravl_node *n)
{
	return ravl_node_cessor(n, RAVL_LEFT);
}

/*
 * ravl_predicate_holds -- (internal) verifies the given predicate for
 *	the current node in the search path
 *
 * If the predicate holds for the given node or a node that can be directly
 * derived from it, returns 1. Otherwise returns 0.
 */
static int
ravl_predicate_holds(int result, struct ravl_node **ret,
	struct ravl_node *n, enum ravl_predicate flags)
{
	if (flags & RAVL_PREDICATE_EQUAL) {
		if (result == 0) {
			*ret = n;
			return 1;
		}
	}
	if (flags & RAVL_PREDICATE_GREATER) {
		if (result < 0) { /* data < n->data */
			*ret = n;
			return 0;
		} else if (result == 0) {
			*ret = ravl_node_successor(n);
			return 1;
		}
	}
	if (flags & RAVL_PREDICATE_LESS) {
		if (result > 0) { /* data > n->data */
			*ret = n;
			return 0;
		} else if (result == 0) {
			*ret = ravl_node_predecessor(n);
			return 1;
		}
	}

	return 0;
}

/*
 * ravl_find -- searches for the node in the tree
 */
struct ravl_node *
ravl_find(struct ravl *ravl, const void *data, enum ravl_predicate flags)
{
	LOG(6, NULL);

	struct ravl_node *r = NULL;
	struct ravl_node *n = ravl->root;
	while (n) {
		int result = ravl->compare(data, ravl_data(n));
		if (ravl_predicate_holds(result, &r, n, flags))
			return r;

		n = n->slots[result > 0];
	}

	return r;
}

/*
 * ravl_remove -- removes the given node from the tree
 */
void
ravl_remove(struct ravl *ravl, struct ravl_node *n)
{
	LOG(6, NULL);

	if (n->slots[RAVL_LEFT] != NULL && n->slots[RAVL_RIGHT] != NULL) {
		/* if both children are present, remove the successor instead */
		struct ravl_node *s = ravl_node_successor(n);
		memcpy(n->data, s->data, ravl->data_size);

		ravl_remove(ravl, s);
	} else {
		/* swap n with the child that may exist */
		struct ravl_node *r = n->slots[RAVL_LEFT] ?
			n->slots[RAVL_LEFT] : n->slots[RAVL_RIGHT];
		if (r != NULL)
			r->parent = n->parent;

		*ravl_node_ref(ravl, n) = r;
		Free(n);
	}
}

/*
 * ravl_data -- returns the data contained within the node
 */
void *
ravl_data(struct ravl_node *node)
{
	if (node->pointer_based) {
		void *data;
		memcpy(&data, node->data, sizeof(void *));
		return data;
	} else {
		return (void *)node->data;
	}
}

/*
 * ravl_first -- returns first (left-most) node in the tree
 */
struct ravl_node *
ravl_first(struct ravl *ravl)
{
	if (ravl->root)
		return ravl_node_type_most(ravl->root, RAVL_LEFT);

	return NULL;
}

/*
 * ravl_last -- returns last (right-most) node in the tree
 */
struct ravl_node *
ravl_last(struct ravl *ravl)
{
	if (ravl->root)
		return ravl_node_type_most(ravl->root, RAVL_RIGHT);

	return NULL;
}
