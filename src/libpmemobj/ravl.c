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
 * ravl.c -- implementation of a RAVL tree:
 * http://sidsen.azurewebsites.net//papers/ravl-trees-journal.pdf
 */

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include "out.h"
#include "ravl.h"

enum ravl_slot_type {
	RAVL_LEFT,
	RAVL_RIGHT,

	MAX_SLOTS,

	RAVL_ROOT
};

struct ravl_node {
	struct ravl_node *parent;
	struct ravl_node *slots[MAX_SLOTS];
	const void *data;
	int rank;
};

struct ravl {
	struct ravl_node *root;
	ravl_compare *compare;
};

/*
 * ravl_new -- creates a new ravl tree instance
 */
struct ravl *
ravl_new(ravl_compare *compare)
{
	struct ravl *r = Malloc(sizeof(*r));
	if (r == NULL)
		return NULL;

	r->compare = compare;
	r->root = NULL;

	return r;
}

static void
ravl_clear_node(struct ravl_node *n, ravl_cb cb, void *arg)
{
	if (n == NULL)
		return;

	ravl_clear_node(n->slots[RAVL_LEFT], cb, arg);
	if (cb)
		cb((void *)n->data, arg);
	ravl_clear_node(n->slots[RAVL_RIGHT], cb, arg);

	Free(n);
}

void
ravl_clear(struct ravl *ravl)
{
	ravl_clear_node(ravl->root, NULL, NULL);
}

void
ravl_delete_cb(struct ravl *ravl, ravl_cb cb, void *arg)
{
	ravl_clear_node(ravl->root, cb, arg);
	Free(ravl);
}

void
ravl_delete(struct ravl *ravl)
{
	ravl_delete_cb(ravl, NULL, NULL);
}

int
ravl_empty(struct ravl *ravl)
{
	return ravl->root == NULL;
}

static struct ravl_node *
ravl_new_node(const void *data)
{
	struct ravl_node *n = Malloc(sizeof(*n));
	if (n == NULL)
		return NULL;

	n->parent = NULL;
	n->slots[RAVL_LEFT] = NULL;
	n->slots[RAVL_RIGHT] = NULL;
	n->rank = 0;
	n->data = data;

	return n;
}

static enum ravl_slot_type
ravl_slot_opposite(enum ravl_slot_type t)
{
	ASSERTne(t, RAVL_ROOT);

	return t == RAVL_LEFT ? RAVL_RIGHT : RAVL_LEFT;
}

static enum ravl_slot_type
ravl_node_slot_type(struct ravl_node *n)
{
	if (n->parent == NULL)
		return RAVL_ROOT;

	return n->parent->slots[RAVL_LEFT] == n ? RAVL_LEFT : RAVL_RIGHT;
}

static struct ravl_node *
ravl_node_sibling(struct ravl_node *n)
{
	enum ravl_slot_type t = ravl_node_slot_type(n);
	if (t == RAVL_ROOT)
		return NULL;

	return n->parent->slots[t == RAVL_LEFT ? RAVL_RIGHT : RAVL_LEFT];
}

static struct ravl_node **
ravl_node_ref(struct ravl *ravl, struct ravl_node *n)
{
	enum ravl_slot_type t = ravl_node_slot_type(n);

	return t == RAVL_ROOT ? &ravl->root : &n->parent->slots[t];
}

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

static int
ravl_node_rank(struct ravl_node *n)
{
	return n == NULL ? -1 : n->rank;
}

static int
ravl_node_rank_difference_parent(struct ravl_node *p, struct ravl_node *n)
{
	return ravl_node_rank(p) - ravl_node_rank(n);
}

static int
ravl_node_rank_difference(struct ravl_node *n)
{
	return ravl_node_rank_difference_parent(n->parent, n);
}

static int
ravl_node_is_i_j(struct ravl_node *n, int i, int j)
{
	return (ravl_node_rank_difference_parent(n, n->slots[RAVL_LEFT]) == i &&
		ravl_node_rank_difference_parent(n, n->slots[RAVL_RIGHT]) == j);
}

static int
ravl_node_is(struct ravl_node *n, int i, int j)
{
	return ravl_node_is_i_j(n, i, j) || ravl_node_is_i_j(n, j, i);
}

static void
ravl_node_promote(struct ravl_node *n)
{
	n->rank += 1;
}

static void
ravl_node_demote(struct ravl_node *n)
{
	n->rank -= 1;
}

static void
ravl_balance(struct ravl *ravl, struct ravl_node *n)
{
	while (n->parent && ravl_node_is(n->parent, 0, 1)) {
		ravl_node_promote(n->parent);
		n = n->parent;
	}

	struct ravl_node *s = ravl_node_sibling(n);
	if (!(ravl_node_rank_difference(n) == 0 &&
		ravl_node_rank_difference_parent(n->parent, s) > 1))
		return;

	struct ravl_node *y = n->parent;
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

int
ravl_insert(struct ravl *ravl, const void *data)
{
	struct ravl_node *n = ravl_new_node(data);
	if (n == NULL)
		return -1;

	struct ravl_node **dstp = &ravl->root;
	struct ravl_node *dst = NULL;
	while (*dstp != NULL) {
		dst = (*dstp);
		int cmp_result = ravl->compare(data, dst->data);
		if (cmp_result == 0)
			return -1;

		dstp = &dst->slots[cmp_result > 0];
	}
	n->parent = dst;
	*dstp = n;

	ravl_balance(ravl, n);

	return 0;
}

static struct ravl_node *
ravl_node_type_most(struct ravl_node *n, enum ravl_slot_type t)
{
	while (n->slots[t] != NULL)
		n = n->slots[t];

	return n;
}

static struct ravl_node *
ravl_node_cessor(struct ravl_node *n, enum ravl_slot_type t)
{
	if (n->slots[t])
		return ravl_node_type_most(n->slots[t], ravl_slot_opposite(t));

	while (n->parent != NULL && n == n->parent->slots[t])
		n = n->parent;

	return n->parent;
}

static struct ravl_node *
ravl_node_successor(struct ravl_node *n)
{
	return ravl_node_cessor(n, RAVL_RIGHT);
}

static struct ravl_node *
ravl_node_predecessor(struct ravl_node *n)
{
	return ravl_node_cessor(n, RAVL_LEFT);
}

static struct ravl_node *
ravl_predicate_holds(struct ravl *ravl, int result,
	struct ravl_node *n, const void *data, enum ravl_predicate flags)
{
	if (flags & RAVL_PREDICATE_EQUAL) {
		if (result == 0)
			return n;
	}
	if (flags & RAVL_PREDICATE_GREATER) {
		if (result < 0) { /* data < n->data */
			/* if this is the first bigger value */
			struct ravl_node *p = ravl_node_predecessor(n);
			if (p == NULL || ravl->compare(data, p->data) > 0)
				return n;
		} else if (result == 0) {
			return ravl_node_successor(n);
		}
	}
	if (flags & RAVL_PREDICATE_LESS) {
		if (result > 0) { /* data > n->data */
			/* if this is the first smaller value */
			struct ravl_node *s = ravl_node_successor(n);
			if (s == NULL || ravl->compare(data, s->data) < 0)
				return n;
		} else if (result == 0)
			return ravl_node_predecessor(n);
	}

	return NULL;
}

struct ravl_node *
ravl_find(struct ravl *ravl, const void *data, enum ravl_predicate flags)
{
	struct ravl_node *r;
	struct ravl_node *n = ravl->root;
	while (n) {
		int result = ravl->compare(data, n->data);
		r = ravl_predicate_holds(ravl, result, n, data, flags);
		if (r != NULL)
			return r;

		n = n->slots[result > 0];
	}

	return NULL;
}

void
ravl_remove(struct ravl *ravl, struct ravl_node *n)
{
	if (n->slots[RAVL_LEFT] != NULL && n->slots[RAVL_RIGHT] != NULL) {
		struct ravl_node *s = ravl_node_successor(n);
		n->data = s->data;

		return ravl_remove(ravl, s);
	} else {
		struct ravl_node *r = n->slots[RAVL_LEFT] ?
			n->slots[RAVL_LEFT] : n->slots[RAVL_RIGHT];
		if (r != NULL)
			r->parent = n->parent;

		*ravl_node_ref(ravl, n) = r;
		Free(n);
	}
}

void *
ravl_data(struct ravl_node *node)
{
	return (void *)node->data;
}
