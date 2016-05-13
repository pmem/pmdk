/*
 * Copyright 2015-2016, Intel Corporation
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
 * ctree.c -- crit-bit tree implementation
 *
 * Crit-bit tree, or as otherwise known, a bitwise trie as implemented here
 * provides good performance for the allocator purpose as well as being
 * fairly simple. Contrary to popular balanced binary trees (rb/avl) it mostly
 * performs reads on nodes during insert.
 *
 * This structure is used throughout the libpmemobj for various tasks, the
 * primary one being to store and retrieve best-fit memory blocks.
 */
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "util.h"
#include "out.h"
#include "sys_util.h"
#include "ctree.h"

#define BIT_IS_SET(n, i) (!!((n) & (1ULL << (i))))

/* internal nodes have LSB of the pointer set, leafs do not */
#define NODE_IS_INTERNAL(node) (BIT_IS_SET((uintptr_t)(node), 0))
#define NODE_INTERNAL_GET(node) ((void *)((char *)(node) - 1))
#define NODE_INTERNAL_SET(d, node) ((d) = (void *)((char *)(node) + 1))

#ifndef CTREE_FAST_RECURSIVE_DELETE
#define CTREE_FAST_RECURSIVE_DELETE 1
#endif

struct node {
	void *slots[2]; /* slots for either internal or leaf nodes */
	unsigned diff;	/* most significant differing bit */
};

struct node_leaf {
	uint64_t key;
	uint64_t value;
};

struct ctree {
	void *root;
	pthread_mutex_t lock;
};

/*
 * find_crit_bit -- (internal) finds the most significant differing bit
 */
static unsigned
find_crit_bit(uint64_t lhs, uint64_t rhs)
{
	/* __builtin_clzll is undefined for 0 */
	uint64_t val = lhs ^ rhs;
	ASSERTne(val, 0);
	return 64 - (unsigned)__builtin_clzll(val) - 1;
}

/*
 * ctree_new -- allocates and initializes crit-bit tree instance
 */
struct ctree *
ctree_new()
{
	struct ctree *t = Malloc(sizeof(*t));
	if (t == NULL)
		return NULL;

	util_mutex_init(&t->lock, NULL);

	t->root = NULL;

	return t;
}

#if	CTREE_FAST_RECURSIVE_DELETE
static void
ctree_free_internal_recursive(void *dst)
{
	if (NODE_IS_INTERNAL(dst)) {
		struct node *a = NODE_INTERNAL_GET(dst);
		ctree_free_internal_recursive(a->slots[0]);
		ctree_free_internal_recursive(a->slots[1]);
		Free(a);
	} else {
		Free(dst);
	}
}
#endif

/*
 * ctree_delete -- cleanups and frees crit-bit tree instance
 */
void
ctree_delete(struct ctree *t)
{
#if	CTREE_FAST_RECURSIVE_DELETE
	if (t->root)
		ctree_free_internal_recursive(t->root);
#else
	while (t->root)
		ctree_remove_unlocked(t, 0, 0);
#endif

	util_mutex_destroy(&t->lock);

	Free(t);
}

/*
 * ctree_insert_unlocked -- inserts a new key into the tree
 */
int
ctree_insert_unlocked(struct ctree *t, uint64_t key, uint64_t value)
{
	void **dst = &t->root;
	struct node *a = NULL;
	int err = 0;

	/* descend the path until a best matching key is found */
	while (NODE_IS_INTERNAL(*dst)) {
		a = NODE_INTERNAL_GET(*dst);
		dst = &a->slots[BIT_IS_SET(key, a->diff)];
	}

	struct node_leaf *dstleaf = *dst;
	struct node_leaf *nleaf = Malloc(sizeof(*nleaf));
	if (nleaf == NULL)
		return ENOMEM;

	nleaf->key = key;
	nleaf->value = value;

	if (dstleaf == NULL) { /* root */
		*dst = nleaf;
		goto out;
	}

	struct node *n = Malloc(sizeof(*n)); /* internal node */
	if (n == NULL) {
		err = ENOMEM;
		goto error_internal_malloc;
	}

	if (dstleaf->key == key) {
		err = EEXIST;
		goto error_duplicate;
	}

	n->diff = find_crit_bit(dstleaf->key, key);

	/* insert the node at the direction based on the critical bit */
	int d = BIT_IS_SET(key, n->diff);
	n->slots[d] = nleaf;

	/* find the appropriate position in the tree to insert the node */
	dst = &t->root;
	while (NODE_IS_INTERNAL(*dst)) {
		a = NODE_INTERNAL_GET(*dst);

		/* the critical bits have to be sorted */
		if (a->diff < n->diff)
			break;
		dst = &a->slots[BIT_IS_SET(key, a->diff)];
	}

	/* insert the found destination in the other slot */
	n->slots[!d] = *dst;
	NODE_INTERNAL_SET(*dst, n);

out:
	return 0;

error_duplicate:
	Free(n);
error_internal_malloc:
	Free(nleaf);
	return err;
}

/*
 * ctree_insert -- inserts a new key into the tree
 */
int
ctree_insert(struct ctree *t, uint64_t key, uint64_t value)
{
	util_mutex_lock(&t->lock);
	int ret = ctree_insert_unlocked(t, key, value);
	util_mutex_unlock(&t->lock);
	return ret;
}

/*
 * ctree_find_unlocked -- searches for an equal key in the tree
 */
uint64_t
ctree_find_unlocked(struct ctree *t, uint64_t key)
{
	struct node_leaf *dst = t->root;
	struct node *a = NULL;

	while (NODE_IS_INTERNAL(dst)) {
		a = NODE_INTERNAL_GET(dst);
		dst = a->slots[BIT_IS_SET(key, a->diff)];
	}

	if (dst && dst->key == key)
		return key;
	else
		return 0;
}

/*
 * ctree_find -- searches for an equal key in the tree
 */
uint64_t
ctree_find(struct ctree *t, uint64_t key)
{
	util_mutex_lock(&t->lock);
	uint64_t ret = ctree_find_unlocked(t, key);
	util_mutex_unlock(&t->lock);
	return ret;
}

/*
 * ctree_find_le_unlocked -- searches for a (less or equal) key in the tree
 */
uint64_t
ctree_find_le_unlocked(struct ctree *t, uint64_t *key)
{
	struct node_leaf *dst = t->root;
	struct node *a = NULL;

	while (NODE_IS_INTERNAL(dst)) {
		a = NODE_INTERNAL_GET(dst);
		dst = a->slots[BIT_IS_SET(*key, a->diff)];
	}

	if (dst == NULL || dst->key == *key)
		goto out;

	unsigned diff = find_crit_bit(dst->key, *key);

	struct node *top = NULL;
	dst = t->root;
	while (NODE_IS_INTERNAL(dst)) {
		a = NODE_INTERNAL_GET(dst);
		if (a->diff < diff)
			break;

		if (BIT_IS_SET(*key, a->diff)) {
			top = a->slots[0];
			dst = a->slots[1];
		} else {
			dst = a->slots[0];
		}
	}

	if (!BIT_IS_SET(*key, diff))
		dst = (struct node_leaf *)top;

	while (NODE_IS_INTERNAL(dst)) {
		a = NODE_INTERNAL_GET(dst);
		dst = a->slots[1];
	}

	if (dst && dst->key > *key)
		dst = NULL;

out:
	*key = dst ? dst->key : 0;
	uint64_t ret = dst ? dst->value : 0;
	return ret;
}

/*
 * ctree_find_le -- searches for a (less or equal) key in the tree
 */
uint64_t
ctree_find_le(struct ctree *t, uint64_t *key)
{
	util_mutex_lock(&t->lock);
	uint64_t ret = ctree_find_le_unlocked(t, key);
	util_mutex_unlock(&t->lock);
	return ret;
}

/*
 * ctree_remove_unlocked -- removes a (greater or equal) key from the tree
 */
uint64_t
ctree_remove_unlocked(struct ctree *t, uint64_t key, int eq)
{
	void **p = NULL; /* parent ref */
	void **dst = &t->root; /* node to remove ref */
	struct node *a = NULL; /* internal node */

	struct node_leaf *l = NULL;
	uint64_t k = 0;

	if (t->root == NULL)
		goto out;

	/* find the key */
	while (NODE_IS_INTERNAL(*dst)) {
		a = NODE_INTERNAL_GET(*dst);
		p = dst;
		dst = &a->slots[BIT_IS_SET(key, a->diff)];
	}

	l = *dst;
	k = l->key;

	if (l->key == key) {
		goto remove;
	} else if (eq && l->key != key) {
		k = 0;
		goto out;
	}

	unsigned diff = find_crit_bit(k, key);

	void **top = NULL; /* top pointer */
	void **topp = NULL; /* top parent */
	p = NULL;
	dst = &t->root;

	while (NODE_IS_INTERNAL(*dst)) {
		a = NODE_INTERNAL_GET(*dst);
		p = dst;

		if (a->diff < diff)
			break;

		if (BIT_IS_SET(key, a->diff)) {
			dst = &a->slots[1];
		} else {
			topp = dst;
			top = &a->slots[1];
			dst = &a->slots[0];
		}
	}

	if (BIT_IS_SET(key, diff)) {
		dst = top;
		p = topp;
		a = p ? NODE_INTERNAL_GET(*p) : NULL;
	}

	if (dst == NULL) {
		k = 0;
		goto out;
	}

	while (NODE_IS_INTERNAL(*dst)) {
		a = NODE_INTERNAL_GET(*dst);
		p = dst;
		dst = &a->slots[0];
	}

	l = *dst;
	k = l->key;

	ASSERT(k > key);

remove:
	/*
	 * If the node that is being removed isn't root then simply swap the
	 * remaining child with the parent.
	 */
	if (a == NULL) {
		Free(*dst);
		*dst = NULL;
	} else {
		*p = a->slots[a->slots[0] == *dst];
		/* Free the internal node and the leaf */
		Free(*dst);
		Free(a);
	}

out:
	return k;
}

/*
 * ctree_remove -- removes a (greater or equal) key from the tree
 */
uint64_t
ctree_remove(struct ctree *t, uint64_t key, int eq)
{
	util_mutex_lock(&t->lock);
	uint64_t ret = ctree_remove_unlocked(t, key, eq);
	util_mutex_unlock(&t->lock);
	return ret;
}

/*
 * ctree_is_empty_unlocked -- checks whether the tree is empty
 */
int
ctree_is_empty_unlocked(struct ctree *t)
{
	return t->root == NULL;
}

/*
 * ctree_is_empty -- checks whether the tree is empty
 */
int
ctree_is_empty(struct ctree *t)
{
	util_mutex_lock(&t->lock);

	int ret = ctree_is_empty_unlocked(t);

	util_mutex_unlock(&t->lock);

	return ret;
}
