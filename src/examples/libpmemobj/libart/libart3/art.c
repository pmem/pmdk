/*
 * Copyright 2017, FUJITSU TECHNOLOGY SOLUTIONS GMBH
 * Copyright 2012, Armon Dadgar. All rights reserved.
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
 * ===========================================================================
 *
 *       Filename:  art.c
 *
 *    Description:  implement ART tree using libpmemobj based on libart
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  andreas.bluemle@itxperts.de
 *                  dieter.kasper@ts.fujitsu.com
 *
 * Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */
/*
 * based on https://github.com/armon/libart/src/art.c
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include "libpmemobj.h"
#include "obj.h"
#include "art.h"

#ifdef __i386__
#include <emmintrin.h>
#else
#ifdef __amd64__
#include <emmintrin.h>
#endif
#endif

/*
 * Macros to manipulate pointer tags
 */
#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void *)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((art_leaf *)((void *)((uintptr_t)x & ~1)))

size_t pmem_art_leaf_type_num;
PMEMoid alloc_leaf(PMEMobjpool *pop, int buffer_size);
PMEMoid make_leaf(PMEMobjpool *pop,
	const unsigned char *key, int key_len, void *value, int val_len);
int fill_leaf(PMEMobjpool *pop, PMEMoid al,
	const unsigned char *key, int key_len, void *value, int val_len);

PMEMoid
alloc_leaf(PMEMobjpool *pop, int buffer_size)
{
	PMEMoid an;

	an = pmemobj_tx_zalloc(sizeof(art_leaf) + buffer_size,
				pmem_art_leaf_type_num);
	return an;
}

int
fill_leaf(PMEMobjpool *pop, PMEMoid al_oid,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	art_leaf *alp;

	/* assert(pmemobj_type_num(al_oid) == art_leaf_type_num); */
	alp = (art_leaf *)pmemobj_direct(al_oid);

	/* assert(alp->buffer_len >= (key_len + val_len)); */

	alp->key_len = key_len;
	alp->val_len = val_len;
	pmemobj_tx_add_range_direct((void *)&(alp->buffer[0]),
		key_len + val_len);
	memcpy((void *)&(alp->buffer[0]), (void *)key, (size_t)key_len);
	memcpy((void *)&(alp->buffer[key_len]), (void *)value, (size_t)val_len);

	return 0;
}

/*
 * Allocates a node of the given type,
 * initializes to zero and sets the type.
 */
static art_node *
alloc_node(uint8_t type)
{
	art_node *n = NULL;

	switch (type) {
	case NODE4:
		n = (art_node *)calloc(1, sizeof(art_node4));
		break;
	case NODE16:
		n = (art_node *)calloc(1, sizeof(art_node16));
		break;
	case NODE48:
		n = (art_node *)calloc(1, sizeof(art_node48));
		break;
	case NODE256:
		n = (art_node *)calloc(1, sizeof(art_node256));
		break;
	default:
		abort();
	}
	if (n != NULL) {
		n->type = type;
	}
	return n;
}

/*
 * Initializes an ART tree
 * @return 0 on success.
 */
int
art_tree_init(art_tree **tp)
{
	art_tree *t;

	pmem_art_leaf_type_num = TOID_TYPE_NUM(art_leaf);

	if (*tp == NULL) {
		t = (art_tree *)malloc(sizeof(art_tree));
		if (t != (art_tree *)NULL) {
			t->root = NULL;
			t->size = 0;
			*tp = t;
		}
	}
	if (*tp != NULL) {
		return 0;
	} else {
		return 1;
	}
}

/*
 * Recursively destroys the tree
 */
static void
destroy_node(PMEMobjpool *pop, art_node *n)
{
	/* Break if null */
	if (!n)
		return;

	/* Special case leafs */
	if (IS_LEAF(n)) {
		free(LEAF_RAW(n));
		return;
	}

	/* Handle each node type */
	int i;
	union {
		art_node4 *p1;
		art_node16 *p2;
		art_node48 *p3;
		art_node256 *p4;
	} p;
	switch (n->type) {
	case NODE4:
		p.p1 = (art_node4 *)n;
		for (i = 0; i < n->num_children; i++) {
			destroy_node(pop, p.p1->children[i]);
		}
		break;

	case NODE16:
		p.p2 = (art_node16 *)n;
		for (i = 0; i < n->num_children; i++) {
			destroy_node(pop, p.p2->children[i]);
		}
		break;

	case NODE48:
		p.p3 = (art_node48 *)n;
		for (i = 0; i < n->num_children; i++) {
			destroy_node(pop, p.p3->children[i]);
		}
		break;

	case NODE256:
		p.p4 = (art_node256 *)n;
		for (i = 0; i < 256; i++) {
			if (p.p4->children[i])
				destroy_node(pop, p.p4->children[i]);
		}
		break;

	default:
		abort();
	}

	/* Free ourself on the way up */
	free(n);
}

/*
 * Destroys an ART tree
 * @return 0 on success.
 */
int
art_tree_destroy(PMEMobjpool *pop, art_tree *t)
{
	destroy_node(pop, t->root);
	return 0;
}

/*
 * Returns the size of the ART tree.
 */

extern inline uint64_t art_size(art_tree *t);

static art_node **
find_child(art_node *n, unsigned char c)
{
	int i, mask, bitfield;
	union {
		art_node4 *p1;
		art_node16 *p2;
		art_node48 *p3;
		art_node256 *p4;
	} p;

	switch (n->type) {
	case NODE4:
		p.p1 = (art_node4 *)n;
		for (i = 0; i < n->num_children; i++) {
			/*
			 * this cast works around a bug in gcc 5.1 when
			 * unrolling loops
			 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
			 */
			if (((unsigned char *)p.p1->keys)[i] == c)
				return &p.p1->children[i];
			}
		break;

	case NODE16:
	{
		p.p2 = (art_node16 *)n;

		/* support non-86 architectures */
#ifdef __i386__
		/* Compare the key to all 16 stored keys */
		__m128i cmp;
		cmp = _mm_cmpeq_epi8(_mm_set1_epi8(c),
			_mm_loadu_si128((__m128i *)p.p2->keys));

		/* Use a mask to ignore children that don't exist */
		mask = (1 << n->num_children) - 1;
		bitfield = _mm_movemask_epi8(cmp) & mask;
#else
#ifdef __amd64__
		/* Compare the key to all 16 stored keys */
		__m128i cmp;
		cmp = _mm_cmpeq_epi8(_mm_set1_epi8(c),
			_mm_loadu_si128((__m128i *)p.p2->keys));

		/* Use a mask to ignore children that don't exist */
		mask = (1 << n->num_children) - 1;
		bitfield = _mm_movemask_epi8(cmp) & mask;
#else
		/* Compare the key to all 16 stored keys */
		unsigned bitfield = 0;
		for (short i = 0; i < 16; ++i) {
			if (p.p2->keys[i] == c)
				bitfield |= (1 << i);
		}

		/* Use a mask to ignore children that don't exist */
		bitfield &= mask;
#endif
#endif

		/*
		 * If we have a match (any bit set) then we can
		 * return the pointer match using ctz to get
		 * the index.
		 */
		if (bitfield)
			return &p.p2->children[__builtin_ctz(bitfield)];
		break;
	}

	case NODE48:
		p.p3 = (art_node48 *)n;
		i = p.p3->keys[c];
		if (i)
			return &p.p3->children[i - 1];
		break;

	case NODE256:
		p.p4 = (art_node256 *)n;
		if (p.p4->children[c])
			return &p.p4->children[c];
		break;

	default:
		abort();
	}
	return NULL;
}

/* Simple inlined if */
static inline int
min(int a, int b)
{
	return (a < b) ? a : b;
}

/*
 * Returns the number of prefix characters shared between
 * the key and node.
 */
static int
check_prefix(const art_node *n,
	const unsigned char *key, int key_len, int depth)
{
	int max_cmp = min(min(n->partial_len, MAX_PREFIX_LEN), key_len - depth);
	int idx;

	for (idx = 0; idx < max_cmp; idx++) {
		if (n->partial[idx] != key[depth + idx])
		return idx;
	}
	return idx;
}

/*
 * Checks if a leaf matches
 * @return 0 on success.
 */
static int
leaf_matches(const art_leaf *n,
	const unsigned char *key, int key_len, int depth)
{
	(void) depth;

	/* Fail if the key lengths are different */
	if (n->key_len != (uint32_t)key_len)
		return 1;

	/* Compare the keys starting at the depth */
	return memcmp(&(n->buffer[0]), key, key_len);
}

/*
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void *
art_search(PMEMobjpool *pop,  const art_tree *t,
	const unsigned char *key, int key_len)
{
	art_node **child;
	art_node *n = t->root;
	art_leaf *al;
	int prefix_len, depth = 0;

	while (n) {
		/* Might be a leaf */
		if (IS_LEAF(n)) {
			PMEMoid leaf_oid;

			leaf_oid.pool_uuid_lo = pop->uuid_lo;
			leaf_oid.off  = (uint64_t)LEAF_RAW(n);
			al = pmemobj_direct(leaf_oid);

			/* Check if the expanded path matches */
			if (!leaf_matches(al, key, key_len, depth)) {
				return &(al->buffer[al->key_len]);
			}
			return NULL;
		}

		/* Bail if the prefix does not match */
		if (n->partial_len) {
			prefix_len = check_prefix(n, key, key_len, depth);
			if (prefix_len != min(MAX_PREFIX_LEN, n->partial_len))
				return NULL;
			depth = depth + n->partial_len;
		}

		/* Recursively search */
		child = find_child(n, key[depth]);
		n = (child) ? *child : NULL;
		depth++;
	}
	return NULL;
}

/*
 * Find the minimum leaf under a node
 */
static art_leaf *
minimum(PMEMobjpool *pop, const art_node *n)
{
	/* Handle base cases */
	if (!n)
		return NULL;
	if (IS_LEAF(n)) {
		PMEMoid leaf_oid;
		art_leaf *al;
		uint64_t leaf_off;

		leaf_off = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		leaf_oid.off  = leaf_off;
		al = pmemobj_direct(leaf_oid);
		return al;
	}

	int idx;
	switch (n->type) {
	case NODE4:
		return minimum(pop, ((const art_node4 *)n)->children[0]);
	case NODE16:
		return minimum(pop, ((const art_node16 *)n)->children[0]);
	case NODE48:
		idx = 0;
		while (!((const art_node48 *)n)->keys[idx]) idx++;
		idx = ((const art_node48 *)n)->keys[idx] - 1;
		return minimum(pop, ((const art_node48 *)n)->children[idx]);
	case NODE256:
		idx = 0;
		while (!((const art_node256 *)n)->children[idx]) idx++;
		return minimum(pop, ((const art_node256 *)n)->children[idx]);
	default:
		abort();
	}
}

/*
 * Find the maximum leaf under a node
 */
static art_leaf *
maximum(PMEMobjpool *pop, const art_node *n)
{
	/* Handle base cases */
	if (!n)
		return NULL;
	if (IS_LEAF(n)) {
		PMEMoid leaf_oid;
		art_leaf *al;
		uint64_t leaf_off;

		leaf_off = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		leaf_oid.off  = leaf_off;
		al = pmemobj_direct(leaf_oid);
		return al;
	}

	int idx;
	switch (n->type) {
	case NODE4:
		return maximum(pop,
			((const art_node4 *)n)->children[n->num_children - 1]);
	case NODE16:
		return maximum(pop,
			((const art_node16 *)n)->children[n->num_children - 1]);
	case NODE48:
		idx = 255;
		while (!((const art_node48 *)n)->keys[idx]) idx--;
		idx = ((const art_node48 *)n)->keys[idx] - 1;
		return maximum(pop, ((const art_node48 *)n)->children[idx]);
	case NODE256:
		idx = 255;
		while (!((const art_node256 *)n)->children[idx]) idx--;
		return maximum(pop, ((const art_node256 *)n)->children[idx]);
	default:
		abort();
	}
}

/*
 * Returns the minimum valued leaf
 */
art_leaf *
art_minimum(PMEMobjpool *pop, art_tree *t)
{
	return minimum(pop, (art_node *)t->root);
}

/*
 * Returns the maximum valued leaf
 */
art_leaf *
art_maximum(PMEMobjpool *pop, art_tree *t)
{
	return maximum(pop, (art_node *)t->root);
}

PMEMoid
make_leaf(PMEMobjpool *pop,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	PMEMoid newleaf;

	newleaf = alloc_leaf(pop, key_len + val_len);
	fill_leaf(pop, newleaf, key, key_len, value, val_len);

	return newleaf;
}

static int
longest_common_prefix(art_leaf *l1, art_leaf *l2, int depth)
{
	unsigned char *key_al1, *key_al2;
	int max_cmp;
	int idx = 0;

	key_al1 = &(l1->buffer[0]);
	key_al2 = &(l2->buffer[0]);

	max_cmp = min(l1->key_len, l2->key_len) - depth;
	for (idx = 0; idx < max_cmp; idx++) {
		if (key_al1[depth + idx] != key_al2[depth + idx])
			return idx;
	}
	return idx;
}

static void
copy_header(art_node *dest, art_node *src)
{
	dest->num_children = src->num_children;
	dest->partial_len = src->partial_len;
	memcpy(dest->partial, src->partial,
		min(MAX_PREFIX_LEN, src->partial_len));
}

static void
add_child256(art_node256 *n, art_node **ref, unsigned char c, void *child)
{
	(void) ref;
	n->n.num_children++;
	n->children[c] = (art_node *)child;
}

static void
add_child48(art_node48 *n, art_node **ref, unsigned char c, void *child)
{
	if (n->n.num_children < 48) {
		int pos = 0;
		while (n->children[pos]) pos++;
		n->children[pos] = (art_node *)child;
		n->keys[c] = pos + 1;
		n->n.num_children++;
	} else {
		art_node256 *new_node = (art_node256 *)alloc_node(NODE256);
		for (int i = 0; i < 256; i++) {
			if (n->keys[i]) {
				new_node->children[i] =
				    n->children[n->keys[i] - 1];
			}
		}
		copy_header((art_node *)new_node, (art_node *)n);
		*ref = (art_node *)new_node;
		free(n);
		add_child256(new_node, ref, c, child);
	}
}

static void
add_child16(art_node16 *n, art_node **ref, unsigned char c, void *child)
{
	if (n->n.num_children < 16) {
		unsigned mask = (1 << n->n.num_children) - 1;

		/* support non-x86 architectures */
#ifdef __i386__
			__m128i cmp;

			/* Compare the key to all 16 stored keys */
			cmp = _mm_cmplt_epi8(_mm_set1_epi8(c),
					_mm_loadu_si128((__m128i *)n->keys));

			/* Use a mask to ignore children that don't exist */
			unsigned bitfield = _mm_movemask_epi8(cmp) & mask;
#else
#ifdef __amd64__
			__m128i cmp;

			/* Compare the key to all 16 stored keys */
			cmp = _mm_cmplt_epi8(_mm_set1_epi8(c),
					_mm_loadu_si128((__m128i *)n->keys));

			/* Use a mask to ignore children that don't exist */
			unsigned bitfield = _mm_movemask_epi8(cmp) & mask;
#else
			/* Compare the key to all 16 stored keys */
			unsigned bitfield = 0;
			for (short i = 0; i < 16; ++i) {
				if (c < n->keys[i])
					bitfield |= (1 << i);
			}

			/* Use a mask to ignore children that don't exist */
			bitfield &= mask;
#endif
#endif

		/* Check if less than any */
		unsigned idx;
		if (bitfield) {
			idx = __builtin_ctz(bitfield);
			memmove(n->keys + idx + 1, n->keys + idx,
				n->n.num_children - idx);
			memmove(n->children + idx + 1, n->children + idx,
				(n->n.num_children - idx) * sizeof(void *));
		} else
			idx = n->n.num_children;

		/* Set the child */
		n->keys[idx] = c;
		n->children[idx] = (art_node *)child;
		n->n.num_children++;

	} else {
		art_node48 *new_node = (art_node48 *)alloc_node(NODE48);

		/* Copy the child pointers and populate the key map */
		memcpy(new_node->children, n->children,
		    sizeof(void *)*n->n.num_children);
		for (int i = 0; i < n->n.num_children; i++) {
			new_node->keys[n->keys[i]] = i + 1;
		}
		copy_header((art_node *)new_node, (art_node *)n);
		*ref = (art_node *)new_node;
		free(n);
		add_child48(new_node, ref, c, child);
	}
}

static void
add_child4(art_node4 *n, art_node **ref, unsigned char c, void *child)
{
	if (n->n.num_children < 4) {
		int idx;
		for (idx = 0; idx < n->n.num_children; idx++) {
			if (c < n->keys[idx])
				break;
		}

		/* Shift to make room */
		memmove(n->keys + idx + 1, n->keys + idx,
			n->n.num_children - idx);
		memmove(n->children + idx + 1, n->children + idx,
			(n->n.num_children - idx) * sizeof(void *));

		/* Insert element */
		n->keys[idx] = c;
		n->children[idx] = (art_node *)child;
		n->n.num_children++;

	} else {
		art_node16 *new_node = (art_node16 *)alloc_node(NODE16);

		/* Copy the child pointers and the key map */
		memcpy(new_node->children, n->children,
				sizeof(void *)*n->n.num_children);
		memcpy(new_node->keys, n->keys,
				sizeof(unsigned char)*n->n.num_children);
		copy_header((art_node *)new_node, (art_node *)n);
		*ref = (art_node *)new_node;
		free(n);
		add_child16(new_node, ref, c, child);
	}
}

static void
add_child(art_node *n, art_node **ref, unsigned char c, void *child)
{
	switch (n->type) {
		case NODE4:
			return add_child4((art_node4 *)n, ref, c, child);
		case NODE16:
			return add_child16((art_node16 *)n, ref, c, child);
		case NODE48:
			return add_child48((art_node48 *)n, ref, c, child);
		case NODE256:
			return add_child256((art_node256 *)n, ref, c, child);
		default:
			abort();
	}
}

/*
 * Calculates the index at which the prefixes mismatch
 */
static int
prefix_mismatch(PMEMobjpool *pop, const art_node *n,
	const unsigned char *key, int key_len, int depth)
{
	int max_cmp = min(min(MAX_PREFIX_LEN, n->partial_len), key_len - depth);
	int idx;
	for (idx = 0; idx < max_cmp; idx++) {
		if (n->partial[idx] != key[depth + idx])
			return idx;
	}

	/* If the prefix is short we can avoid finding a leaf */
	if (n->partial_len > MAX_PREFIX_LEN) {
		/* Prefix is longer than what we've checked, find a leaf */
		art_leaf *l = minimum(pop, n);
		max_cmp = min(l->key_len, key_len) - depth;
		for (; idx < max_cmp; idx++) {
			if (l->buffer[idx + depth] != key[depth + idx])
				return idx;
		}
	}
	return idx;
}

static PMEMoid
recursive_insert(PMEMobjpool *pop, art_node *n, art_node **ref,
	const unsigned char *key, int key_len,
	void *value, int val_len,
	int depth, int *old)
{
	PMEMoid leaf_oid;
	art_leaf *alp;
	uint64_t leaf_off;

	/* If we are at a NULL node, inject a leaf */
	if (!n) {
		leaf_oid = make_leaf(pop, key, key_len, value, val_len);
		leaf_off = leaf_oid.off;
		*ref = (art_node *)SET_LEAF(leaf_off);
		return leaf_oid;
	}

	/* If we are at a leaf, we need to replace it with a node */
	if (IS_LEAF(n)) {
		leaf_off = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		leaf_oid.off  = leaf_off;
		alp = pmemobj_direct(leaf_oid);


		/* Check if we are updating an existing value */
		if (!leaf_matches(alp, key, key_len, depth)) {
			*old = 1;
			/*
			 * XXX this will not work:
			 * and overwrite of the value is possible iff the
			 * new value has the same size as the old value.
			 * If it doesn't, then we need to remove the existing
			 * leaf from the list, allocate a complete new
			 * leaf and insert that into the list again.
			 */
			memcpy(&(alp->buffer[alp->key_len]), value, val_len);
			return OID_NULL;
		}

		/* New value, we must split the leaf into a node4 */
		art_node4 *new_node = (art_node4 *)alloc_node(NODE4);

		/* Create a new leaf */
		PMEMoid leaf2_oid =
			make_leaf(pop, key, key_len, value, val_len);
		art_leaf *al2p = pmemobj_direct(leaf2_oid);
		uint64_t leaf2_off = leaf2_oid.off;

		/* Determine longest prefix */
		int longest_prefix = longest_common_prefix(alp, al2p, depth);
		new_node->n.partial_len = longest_prefix;
		memcpy(new_node->n.partial, key + depth,
			min(MAX_PREFIX_LEN, longest_prefix));
		/* Add the leafs to the new node4 */
		*ref = (art_node *)new_node;
		add_child4(new_node, ref, alp->buffer[depth + longest_prefix],
			SET_LEAF(leaf_off));
		add_child4(new_node, ref, al2p->buffer[depth + longest_prefix],
			SET_LEAF(leaf2_off));
		return leaf2_oid;
	}

	/* Check if given node has a prefix */
	if (n->partial_len) {
		/* Determine if the prefixes differ, since we need to split */
		int prefix_diff = prefix_mismatch(pop, n, key, key_len, depth);
		if ((uint32_t)prefix_diff >= n->partial_len) {
			depth += n->partial_len;
			goto RECURSE_SEARCH;
		}

		/* Create a new node */
		art_node4 *new_node = (art_node4 *)alloc_node(NODE4);
		*ref = (art_node *)new_node;
		new_node->n.partial_len = prefix_diff;
		memcpy(new_node->n.partial, n->partial,
			min(MAX_PREFIX_LEN, prefix_diff));

		/* Adjust the prefix of the old node */
		if (n->partial_len <= MAX_PREFIX_LEN) {
			add_child4(new_node, ref, n->partial[prefix_diff], n);
			n->partial_len -= (prefix_diff + 1);
			memmove(n->partial, n->partial + prefix_diff + 1,
					min(MAX_PREFIX_LEN, n->partial_len));
		} else {
			unsigned char *dst;
			const unsigned char *src;
			size_t len;

			n->partial_len -= (prefix_diff + 1);
			art_leaf *l = minimum(pop, n);
			add_child4(new_node, ref,
				l->buffer[depth + prefix_diff], n);
			dst = n->partial;
			src = &(l->buffer[depth + prefix_diff + 1]);
			len = min(MAX_PREFIX_LEN, n->partial_len);

			memcpy(dst, src, len);
		}

		/* Insert the new leaf */
		leaf_oid = make_leaf(pop, key, key_len, value, val_len);
		leaf_off = leaf_oid.off;
		add_child4(new_node, ref, key[depth + prefix_diff],
			SET_LEAF(leaf_off));
		return leaf_oid;
	}

RECURSE_SEARCH:;

	/* Find a child to recurse to */
	art_node **child = find_child(n, key[depth]);
	if (child) {
		return recursive_insert(pop, *child, child,
			key, key_len, value, val_len, depth + 1, old);
	}

	/* No child, node goes within us */
	leaf_oid = make_leaf(pop, key, key_len, value, val_len);
	leaf_off = leaf_oid.off;
	add_child(n, ref, key[depth], SET_LEAF(leaf_off));
	return leaf_oid;
}

static PMEMoid
recursive_insert_leaf(PMEMobjpool *pop, art_node *n, art_node **ref,
	int depth, TOID(art_leaf) new_leaf)
{
	PMEMoid leaf_oid;
	uint64_t leaf_off;
	art_leaf *alp;
	art_leaf *new_alp;

	/* If we are at a NULL node, inject a leaf */
	if (!n) {
		*ref = (art_node *)SET_LEAF(new_leaf.oid.off);
		return new_leaf.oid;
	}

	new_alp = pmemobj_direct(new_leaf.oid);
	/* If we are at a leaf, we need to replace it with a node */
	if (IS_LEAF(n)) {
		leaf_off = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		leaf_oid.off  = leaf_off;
		alp = pmemobj_direct(leaf_oid);

		/* Check if we are updating an existing value */
		if (!leaf_matches(alp, &(new_alp->buffer[0]),
			new_alp->key_len, depth)) {
			/*
			 * XXX this will not work:
			 * and overwrite of the value is possible iff the
			 * new value has the same size as the old value.
			 * If it doesn't, then we need to remove the existing
			 * leaf from the list, allocate a complete new
			 * leaf and insert that into the list again.
			 */
			memcpy(&(alp->buffer[alp->key_len]),
				&(new_alp->buffer[new_alp->key_len]),
				new_alp->val_len);
			return OID_NULL;
		}

		/* New value, we must split the leaf into a node4 */
		art_node4 *new_node = (art_node4 *)alloc_node(NODE4);

		/* Determine longest prefix */
		int longest_prefix = longest_common_prefix(alp, new_alp, depth);
		new_node->n.partial_len = longest_prefix;
		memcpy(new_node->n.partial, &(new_alp->buffer[depth]),
			min(MAX_PREFIX_LEN, longest_prefix));
		/* Add the leafs to the new node4 */
		*ref = (art_node *)new_node;
		add_child4(new_node, ref,
			alp->buffer[depth + longest_prefix],
			SET_LEAF(leaf_off));
		add_child4(new_node, ref,
			new_alp->buffer[depth + longest_prefix],
			SET_LEAF(new_leaf.oid.off));
		return new_leaf.oid;
	}

	/* Check if given node has a prefix */
	if (n->partial_len) {
		/* Determine if the prefixes differ, since we need to split */
		int prefix_diff = prefix_mismatch(pop, n, &(new_alp->buffer[0]),
					new_alp->key_len, depth);
		if ((uint32_t)prefix_diff >= n->partial_len) {
			depth += n->partial_len;
			goto RECURSE_SEARCH;
		}

		/* Create a new node */
		art_node4 *new_node = (art_node4 *)alloc_node(NODE4);
		*ref = (art_node *)new_node;
		new_node->n.partial_len = prefix_diff;
		memcpy(new_node->n.partial, n->partial,
			min(MAX_PREFIX_LEN, prefix_diff));

		/* Adjust the prefix of the old node */
		if (n->partial_len <= MAX_PREFIX_LEN) {
			add_child4(new_node, ref, n->partial[prefix_diff], n);
			n->partial_len -= (prefix_diff + 1);
			memmove(n->partial, n->partial + prefix_diff + 1,
					min(MAX_PREFIX_LEN, n->partial_len));
		} else {
			unsigned char *dst;
			const unsigned char *src;
			size_t len;

			n->partial_len -= prefix_diff + 1;
			art_leaf *l = minimum(pop, n);
			add_child4(new_node, ref,
				l->buffer[depth + prefix_diff], n);
			dst = n->partial;
			src = &(l->buffer[depth + prefix_diff + 1 ]);
			len = min(MAX_PREFIX_LEN, n->partial_len);

			memcpy(dst, src, len);
		}

		/* Insert the new leaf */
		add_child4(new_node, ref,
			new_alp->buffer[depth + prefix_diff],
			SET_LEAF(new_leaf.oid.off));
		return new_leaf.oid;
	}

RECURSE_SEARCH:;

	/* Find a child to recurse to */
	art_node **child = find_child(n, new_alp->buffer[depth]);
	if (child) {
		return recursive_insert_leaf(pop, *child, child,
				depth + 1, new_leaf);
	}

	/* No child, node goes within us */
	add_child(n, ref, new_alp->buffer[depth], SET_LEAF(new_leaf.oid.off));
	return new_leaf.oid;
}

int
art_rebuild_tree_from_pmem_list(PMEMobjpool *pop, art_tree *t)
{
	TOID(struct pmem_art_tree_root) root;
	TOID(art_leaf) leaf;

	root = POBJ_ROOT(pop, struct pmem_art_tree_root);
	POBJ_LIST_FOREACH(leaf, &D_RO(root)->qhead, entries) {
		art_insert_leaf(pop, t, leaf);
	}
	return 0;
}

void *
art_insert_leaf(PMEMobjpool *pop, art_tree *t, TOID(art_leaf) leaf)
{
	recursive_insert_leaf(pop, t->root, &t->root, 0, leaf);
	t->size++;
	return NULL;
}

/*
 * Inserts a new value into the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @arg value Opaque value.
 * @return NULL if the item was newly inserted, otherwise
 * the old value pointer is returned.
 */
void *
art_insert(PMEMobjpool *pop, art_tree *t,
	const unsigned char *key, int key_len,
	void *value, int val_len)
{
	int old_val = 0;
	PMEMoid new_leaf_oid;
	TOID(art_leaf) typed_leaf_oid;
	TOID(struct pmem_art_tree_root) root;

	TX_BEGIN(pop) {
		root = POBJ_ROOT(pop, struct pmem_art_tree_root);
		TX_ADD(root);

		new_leaf_oid = recursive_insert(pop, t->root, &t->root,
				key, key_len, value, val_len, 0, &old_val);
		typed_leaf_oid.oid = new_leaf_oid;
		if (!OID_IS_NULL(new_leaf_oid)) {
			t->size++;
			POBJ_LIST_INSERT_HEAD(pop, &D_RW(root)->qhead,
				typed_leaf_oid, entries);
		}
	} TX_ONABORT {
		abort();
	} TX_END
	return (void *)(new_leaf_oid.off);
}

static void
remove_child256(art_node256 *n, art_node **ref, unsigned char c)
{
	n->children[c] = NULL;
	n->n.num_children--;

	/* Resize to a node48 on underflow, not immediately to prevent */
	/* trashing if we sit on the 48/49 boundary */
	if (n->n.num_children == 37) {
		art_node48 *new_node = (art_node48 *)alloc_node(NODE48);
		*ref = (art_node *)new_node;
		copy_header((art_node *)new_node, (art_node *)n);

		int pos = 0;
		for (int i = 0; i < 256; i++) {
			if (n->children[i]) {
				new_node->children[pos] = n->children[i];
				new_node->keys[i] = pos + 1;
				pos++;
			}
		}
		free(n);
	}
}

static void
remove_child48(art_node48 *n, art_node **ref, unsigned char c)
{
	int pos = n->keys[c];
	n->keys[c] = 0;
	n->children[pos - 1] = NULL;
	n->n.num_children--;

	if (n->n.num_children == 12) {
		art_node16 *new_node = (art_node16 *)alloc_node(NODE16);
		*ref = (art_node *)new_node;
		copy_header((art_node *)new_node, (art_node *)n);

		int child = 0;
		for (int i = 0; i < 256; i++) {
			pos = n->keys[i];
			if (pos) {
				new_node->keys[child] = i;
				new_node->children[child] =
					n->children[pos - 1];
				child++;
			}
		}
		free(n);
	}
}

static void
remove_child16(art_node16 *n, art_node **ref, art_node **l)
{
	int pos = l - n->children;
	memmove(n->keys + pos, n->keys + pos + 1, n->n.num_children - 1 - pos);
	memmove(n->children + pos, n->children + pos + 1,
		(n->n.num_children - 1 - pos) * sizeof(void *));
	n->n.num_children--;

	if (n->n.num_children == 3) {
		art_node4 *new_node = (art_node4 *)alloc_node(NODE4);
		*ref = (art_node *)new_node;
		copy_header((art_node *)new_node, (art_node *)n);
		memcpy(new_node->keys, n->keys, 4);
		memcpy(new_node->children, n->children, 4 * sizeof(void *));
		free(n);
	}
}

static void
remove_child4(art_node4 *n, art_node **ref, art_node **l)
{
	int pos = l - n->children;
	memmove(n->keys + pos, n->keys + pos + 1,
		n->n.num_children - 1 - pos);
	memmove(n->children + pos, n->children + pos + 1,
		(n->n.num_children - 1 - pos)*sizeof(void *));
	n->n.num_children--;

	/* Remove nodes with only a single child */
	if (n->n.num_children == 1) {
		art_node *child = n->children[0];
		if (!IS_LEAF(child)) {
			/* Concatenate the prefixes */
			int prefix = n->n.partial_len;
			if (prefix < MAX_PREFIX_LEN) {
				n->n.partial[prefix] = n->keys[0];
				prefix++;
			}
			if (prefix < MAX_PREFIX_LEN) {
				int sub_prefix = min(child->partial_len,
						MAX_PREFIX_LEN - prefix);
				memcpy(n->n.partial + prefix,
					child->partial, sub_prefix);
				prefix += sub_prefix;
			}

			/* Store the prefix in the child */
			memcpy(child->partial, n->n.partial,
				min(prefix, MAX_PREFIX_LEN));
			child->partial_len += n->n.partial_len + 1;
		}
		*ref = child;
		free(n);
	}
}

static void
remove_child(art_node *n, art_node **ref, unsigned char c, art_node **l)
{
	switch (n->type) {
		case NODE4:
			return remove_child4((art_node4 *)n, ref, l);
		case NODE16:
			return remove_child16((art_node16 *)n, ref, l);
		case NODE48:
			return remove_child48((art_node48 *)n, ref, c);
		case NODE256:
			return remove_child256((art_node256 *)n, ref, c);
		default:
			abort();
	}
}

static PMEMoid
recursive_delete(PMEMobjpool *pop, art_node *n, art_node **ref,
	const unsigned char *key, int key_len, int depth)
{
	PMEMoid leaf_oid;
	uint64_t leaf_off;
	art_leaf *alp;

	/* Search terminated */
	if (!n)
		return OID_NULL;

	/* Handle hitting a leaf node */
	if (IS_LEAF(n)) {
		leaf_off = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		leaf_oid.off  = leaf_off;
		alp = pmemobj_direct(leaf_oid);
		if (!leaf_matches(alp, key, key_len, depth)) {
			*ref = NULL;
			return leaf_oid;
		}
		return OID_NULL;
	}

	/* Bail if the prefix does not match */
	if (n->partial_len) {
		int prefix_len = check_prefix(n, key, key_len, depth);
		if (prefix_len != min(MAX_PREFIX_LEN, n->partial_len)) {
			return OID_NULL;
		}
		depth = depth + n->partial_len;
	}

	/* Find child node */
	art_node **child = find_child(n, key[depth]);
	if (!child)
		return OID_NULL;

	/* If the child is leaf, delete from this node */
	if (IS_LEAF(*child)) {
		leaf_off = (uint64_t)LEAF_RAW(*child);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		leaf_oid.off  = leaf_off;
		alp = pmemobj_direct(leaf_oid);
		if (!leaf_matches(alp, key, key_len, depth)) {
			remove_child(n, ref, key[depth], child);
			return leaf_oid;
		}
		return OID_NULL;

	/* Recurse */
	} else {
		return recursive_delete(pop, *child,
		    child, key, key_len, depth + 1);
	}
}

/*
 * Deletes a value from the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void *
art_delete(PMEMobjpool *pop, art_tree *t, const unsigned char *key, int key_len)
{
	void *old = NULL;
	TOID(art_leaf) typed_leaf_oid;
	PMEMoid loid;
	art_leaf *alp;
	TOID(struct pmem_art_tree_root) root;

	TX_BEGIN(pop) {
		root = POBJ_ROOT(pop, struct pmem_art_tree_root);
		TX_ADD(root);
		loid = recursive_delete(pop, t->root,
			    &t->root, key, key_len, 0);
		if (!OID_IS_NULL(loid)) {
			t->size--;
			alp = (art_leaf *)pmemobj_direct(loid);
			old = (void *)malloc(alp->val_len);
			memcpy(old, &(alp->buffer[alp->key_len]), alp->val_len);

			typed_leaf_oid.oid = loid;
			POBJ_LIST_REMOVE(pop, &D_RW(root)->qhead,
				typed_leaf_oid, entries);

			pmemobj_tx_free(loid);
		}
	} TX_ONABORT {
		abort();
	} TX_END

	return old;
}

/*
 * Recursively iterates over the tree
 */
static int
recursive_iter(PMEMobjpool *pop, art_node *n, art_callback cb, void *data)
{
	cb_data cbd;

	cbd.node = n;
	cbd.child_idx = -1;
	/* Handle base cases */
	if (!n)
		return 0;

	if (IS_LEAF(n)) {
		PMEMoid leaf_oid;

		leaf_oid.off  = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		cbd.node = (art_node *)pmemobj_direct(leaf_oid);
		return cb(&cbd, NULL, 0, NULL, 0);
	}

	int idx, res;
	switch (n->type) {
	case NODE4:
		for (int i = 0; i < n->num_children; i++) {
			res = recursive_iter(pop,
				((art_node4 *)n)->children[i],
				cb, data);
			if (res)
				return res;
		}
		break;

	case NODE16:
		for (int i = 0; i < n->num_children; i++) {
			res = recursive_iter(pop,
				((art_node16 *)n)->children[i],
				cb, data);
			if (res)
				return res;
		}
		break;

	case NODE48:
		for (int i = 0; i < 256; i++) {
			idx = ((art_node48 *)n)->keys[i];
			if (!idx)
				continue;

			res = recursive_iter(pop,
				((art_node48 *)n)->children[idx - 1],
				cb, data);
			if (res)
				return res;
		}
		break;

	case NODE256:
		for (int i = 0; i < 256; i++) {
			if (!((art_node256 *)n)->children[i])
				continue;
			res = recursive_iter(pop,
				((art_node256 *)n)->children[i],
				cb, data);
			if (res)
				return res;
		}
		break;

	default:
		abort();
	}
	return 0;
}

/*
 * Recursively iterates over the tree
 */
static int
recursive_iter2(PMEMobjpool *pop, art_node *n, art_callback cb, void *data)
{
	cb_data cbd;

	cbd.node = n;
	cbd.child_idx = -1;
	/* Handle base cases */
	if (!n)
		return 0;

	if (IS_LEAF(n)) {
		PMEMoid leaf_oid;

		leaf_oid.off  = (uint64_t)LEAF_RAW(n);
		leaf_oid.pool_uuid_lo = pop->uuid_lo;
		cbd.node = (art_node *)pmemobj_direct(leaf_oid);
		return cb(&cbd, NULL, 0, NULL, 0);
	}

	int idx, res;
	switch (n->type) {
	case NODE4:
		for (int i = 0; i < n->num_children; i++) {
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(pop,
				((art_node4 *)n)->children[i],
				cb, data);
			if (res)
				return res;
		}
		break;

	case NODE16:
		for (int i = 0; i < n->num_children; i++) {
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(pop,
				((art_node16 *)n)->children[i],
				cb, data);
			if (res)
				return res;
		}
		break;

	case NODE48:
		for (int i = 0; i < 256; i++) {
			idx = ((art_node48 *)n)->keys[i];
			if (!idx)
				continue;

			cbd.child_idx = idx - 1;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(pop,
				((art_node48 *)n)->children[idx - 1],
				cb, data);
			if (res)
				return res;
		}
		break;

	case NODE256:
		for (int i = 0; i < 256; i++) {
			if (!((art_node256 *)n)->children[i])
				continue;
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(pop,
				((art_node256 *)n)->children[i],
				cb, data);
			if (res)
				return res;
		}
		break;

	default:
		abort();
	}
	return 0;
}

int
art_iter_list(PMEMobjpool *pop, art_callback cb, void *data)
{
	TOID(struct pmem_art_tree_root) root;
	TOID(art_leaf) typed_leaf_oid;
	cb_data cbd;

	root = POBJ_ROOT(pop, struct pmem_art_tree_root);
	POBJ_LIST_FOREACH(typed_leaf_oid, &D_RO(root)->qhead, entries) {
		cbd.node = (art_node *)pmemobj_direct(typed_leaf_oid.oid);
		cbd.child_idx = -1;
		cb(&cbd, NULL, 0, NULL, 0);
	}
	return 0;
}

/*
 * Iterates through the entries pairs in the map,
 * invoking a callback for each. The call back gets a
 * key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int
art_iter(PMEMobjpool *pop, art_tree *t, art_callback cb, void *data)
{
	return recursive_iter(pop, t->root, cb, data);
}

int
art_iter2(PMEMobjpool *pop, art_tree *t, art_callback cb, void *data)
{
	return recursive_iter2(pop, t->root, cb, data);
}

/*
 * Checks if a leaf prefix matches
 * @return 0 on success.
 */
static int
leaf_prefix_matches(const art_leaf *n,
	const unsigned char *prefix, int prefix_len)
{
	/* Fail if the key length is too short */
	if (n->key_len < (uint32_t)prefix_len)
		return 1;

	/* Compare the keys */
	return memcmp(&(n->buffer[0]), prefix, prefix_len);
}

/*
 * Iterates through the entries pairs in the map,
 * invoking a callback for each that matches a given prefix.
 * The call back gets a key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg prefix The prefix of keys to read
 * @arg prefix_len The length of the prefix
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int
art_iter_prefix(PMEMobjpool *pop, art_tree *t,
	const unsigned char *key, int key_len,
	art_callback cb, void *data)
{
	art_node **child;
	art_node *n = t->root;
	int prefix_len, depth = 0;
	while (n) {
		/* Might be a leaf */
		if (IS_LEAF(n)) {
			n = (art_node *)LEAF_RAW(n);
			/* Check if the expanded path matches */
			if (!leaf_prefix_matches((art_leaf *)n, key, key_len)) {
				art_leaf *l = (art_leaf *)n;
				return cb(data,
					(const unsigned char *)&(l->buffer[0]),
					l->key_len,
					&(l->buffer[l->key_len]),
					l->key_len);
			}
			return 0;
		}

		/*
		 * If the depth matches the prefix,
		 * we need to handle this node
		 */
		if (depth == key_len) {
			art_leaf *l = minimum(pop, n);
			if (!leaf_prefix_matches(l, key, key_len))
				return recursive_iter(pop, n, cb, data);
			return 0;
		}

		/* Bail if the prefix does not match */
		if (n->partial_len) {
			prefix_len = prefix_mismatch(pop, n,
				key, key_len, depth);

			/* Guard if the mis-match is longer */
			/* than the MAX_PREFIX_LEN */
			if ((uint32_t)prefix_len > n->partial_len) {
				prefix_len = n->partial_len;
			}

			/* If there is no match, search is terminated */
			if (!prefix_len) {
				return 0;

			/* If we've matched the prefix, iterate on this node */
			} else if (depth + prefix_len == key_len) {
				return recursive_iter(pop, n, cb, data);
			}

			/* if there is a full match, go deeper */
			depth = depth + n->partial_len;
		}

		/* Recursively search */
		child = find_child(n, key[depth]);
		n = (child) ? *child : NULL;
		depth++;
	}
	return 0;
}
