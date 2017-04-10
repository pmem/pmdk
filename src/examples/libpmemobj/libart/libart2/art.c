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
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 * ============================================================================
 */

/*
 * based on https://github.com/armon/libart/src/art.c
 */

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <fcntl.h>
#include <emmintrin.h>
#include <sys/types.h>
#include "libpmemobj.h"
#include "art.h"

PMEMoid null_oid;

size_t art_tree_root_type_num;
size_t art_node4_type_num;
size_t art_node16_type_num;
size_t art_node48_type_num;
size_t art_node256_type_num;
size_t art_leaf_type_num;

int art_tree_init(PMEMobjpool *pop, int *newpool);
PMEMoid make_leaf(PMEMobjpool *pop, const unsigned char *key,
			    int key_len, void *value, int val_len);
int fill_leaf(PMEMobjpool *pop, PMEMoid al,
	const unsigned char *key, int key_len, void *value, int val_len);
PMEMoid alloc_node(PMEMobjpool *pop, art_node_type node_type, int buffer_size);

static int leaf_matches(art_leaf *n, const unsigned char *key,
			    int key_len, int depth);
static int longest_common_prefix(PMEMoid l1, PMEMoid l2,
			    int depth);
static int prefix_mismatch(PMEMoid n, unsigned char *key,
			    int key_len, int depth);
#ifdef LIBART_ITER_PREFIX
static int leaf_prefix_matches(TOID(art_leaf) n,
			    const unsigned char *prefix, int prefix_len);
#endif

static PMEMoid minimum(PMEMoid n_u, uint64_t type_num);
static PMEMoid maximum(PMEMoid n_u);
static void copy_header(art_node *dest, art_node *src);

void *art_insert(PMEMobjpool *pop, const unsigned char *key,
			    int key_len, void *value, int val_len);
static void *recursive_insert(PMEMobjpool *pop,
			    PMEMoid n, PMEMoid *ref,
			    const unsigned char *key, int key_len,
			    void *value, int val_len, int depth, int *old_val);
static void add_child(PMEMobjpool *pop, PMEMoid n, uint64_t type_num,
			    PMEMoid *ref, unsigned char c,
			    PMEMoid child);
static void add_child4(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c,
			    PMEMoid child);
static void add_child16(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c,
			    PMEMoid child);
static void add_child48(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c,
			    PMEMoid child);
static void add_child256(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c,
			    PMEMoid child);
void *art_delete(PMEMobjpool *pop, const unsigned char *key,
			    int key_len);
static PMEMoid recursive_delete(PMEMobjpool *pop,
			    PMEMoid n, PMEMoid *ref,
			    const unsigned char *key, int key_len, int depth);
static void remove_child(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c,
			    PMEMoid *l);
static void remove_child4(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, PMEMoid *l);
static void remove_child16(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, PMEMoid *l);
static void remove_child48(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c);
static void remove_child256(PMEMobjpool *pop, PMEMoid n,
			    PMEMoid *ref, unsigned char c);

static PMEMoid *find_child(PMEMoid n, uint64_t type_num, unsigned char c);

static int check_prefix(const art_node *n, const unsigned char *key,
			    int key_len, int depth);

PMEMoid art_minimum(TOID(struct art_tree_root) t);
PMEMoid art_maximum(TOID(struct art_tree_root) t);

static void destroy_node(PMEMoid n);
int art_iter(PMEMobjpool *pop, art_callback cb, void *data);
int art_iter2(PMEMobjpool *pop, art_callback cb, void *data);

static void PMEMOIDcopy(PMEMoid *dest, const PMEMoid *src, const int n);
static void PMEMOIDmove(PMEMoid *dest, PMEMoid *src, const int n);

static void
PMEMOIDcopy(PMEMoid *dest, const PMEMoid *src, const int n)
{
	int i;

	for (i = 0; i < n; i++) {
		dest[i] = src[i];
	}
}

static void
PMEMOIDmove(PMEMoid *dest, PMEMoid *src, const int n)
{
	int i;

	if (dest > src) {
		for (i = n - 1; i >= 0; i--) {
			dest[i] = src[i];
		}
	} else {
		for (i = 0; i < n; i++) {
			dest[i] = src[i];
		}
	}
}

PMEMoid
alloc_node(PMEMobjpool *pop, art_node_type node_type, int buffer_size)
{
	PMEMoid an = null_oid;
	art_leaf *alp;

	switch (node_type) {
	case NODE4:
		an = pmemobj_tx_zalloc(sizeof(art_node4),
				    art_node4_type_num);
		break;
	case NODE16:
		an = pmemobj_tx_zalloc(sizeof(art_node16),
				    art_node16_type_num);
		break;
	case NODE48:
		an = pmemobj_tx_zalloc(sizeof(art_node48),
				    art_node48_type_num);
		break;
	case NODE256:
		an = pmemobj_tx_zalloc(sizeof(art_node256),
				    art_node256_type_num);
		break;
	case art_leaf_t:
		an = pmemobj_tx_zalloc(sizeof(art_leaf) + buffer_size,
				    art_leaf_type_num);
		alp = pmemobj_direct(an);
		alp->buffer_len = buffer_size;
		break;
	default:
		/* invalid node type */
		break;
	}

	return an;
}

int
art_tree_init(PMEMobjpool *pop, int *newpool)
{
	int errors = 0;
	TOID(struct art_tree_root) root;

	if (pop == NULL) {
		errors++;
		goto out;
	}

	null_oid = OID_NULL;

	art_tree_root_type_num = TOID_TYPE_NUM(struct art_tree_root);
	art_node4_type_num = TOID_TYPE_NUM(art_node4);
	art_node16_type_num = TOID_TYPE_NUM(art_node16);
	art_node48_type_num = TOID_TYPE_NUM(art_node48);
	art_node256_type_num = TOID_TYPE_NUM(art_node256);
	art_leaf_type_num = TOID_TYPE_NUM(art_leaf);

	TX_BEGIN(pop) {
		root = POBJ_ROOT(pop, struct art_tree_root);
		if (*newpool) {
			TX_ADD(root);
			D_RW(root)->root = OID_NULL;
			D_RW(root)->size = 0;
			*newpool = 0;
		} else {
			errors++;
		}
	} TX_END

out:
	return errors;
}

/*
 * Recursively destroys the tree
 */
static void
destroy_node(PMEMoid n)
{
	uint64_t type_num;
	art_node4 *an4;
	art_node16 *an16;
	art_node48 *an48;
	art_node256 *an256;

	/* Break if null */
	if (OID_IS_NULL(n))
		return;

	type_num = pmemobj_type_num(n);
	/* Special case leafs */
	if (type_num == art_leaf_type_num) {
		pmemobj_tx_free(n);
		return;
	}

	/* Handle each node type */
	int i;


	if (type_num == art_node4_type_num) {
		an4 = (art_node4 *)pmemobj_direct(n);
		for (i = 0; i < an4->n.num_children; i++) {
			destroy_node(an4->children[i]);
		}
	} else if (type_num == art_node16_type_num) {
		an16 = (art_node16 *)pmemobj_direct(n);
		for (i = 0; i < an16->n.num_children; i++) {
			destroy_node(an16->children[i]);
		}
	} else if (type_num == art_node48_type_num) {
		an48 = (art_node48 *)pmemobj_direct(n);
		for (i = 0; i < an48->n.num_children; i++) {
			destroy_node(an48->children[i]);
		}
	} else if (type_num == art_node256_type_num) {
		an256 = (art_node256 *)pmemobj_direct(n);
		for (i = 0; i < 256; i++) {
			if (!(OID_IS_NULL(an256->children[i]))) {
				destroy_node(an256->children[i]);
			}
		}
	} else {
		abort();
	}

	/* Free ourself on the way up */
	pmemobj_tx_free(n);
}

/*
 * Destroys an ART tree
 * @return 0 on success.
 */
int
art_tree_destroy(TOID(struct art_tree_root) t)
{
	destroy_node(D_RO(t)->root);
	return 0;
}

static PMEMoid *
find_child(PMEMoid n, uint64_t type_num, unsigned char c)
{
	int i;
	int mask;
	int bitfield;
	art_node4    *an4;
	art_node16   *an16;
	art_node48   *an48;
	art_node256  *an256;

	if (type_num == art_node4_type_num) {
		an4 = (art_node4 *)pmemobj_direct(n);
		for (i = 0; i < an4->n.num_children; i++) {
			if (an4->keys[i] == c) {
				return &(an4->children[i]);
			}
		}
	} else if (type_num == art_node16_type_num) {
		__m128i cmp;
		an16 = (art_node16 *)pmemobj_direct(n);

		/* Compare the key to all 16 stored keys */
		cmp = _mm_cmpeq_epi8(_mm_set1_epi8(c),
			    _mm_loadu_si128((__m128i *)an16->keys));

		/* Use a mask to ignore children that don't exist */
		mask = (1 << an16->n.num_children) - 1;
		bitfield = _mm_movemask_epi8(cmp) & mask;

		/*
		 * If we have a match (any bit set) then we can
		 * return the pointer match using ctz to get the index.
		 */
		if (bitfield) {
			return &(an16->children[__builtin_ctz(bitfield)]);
		}
	} else if (type_num == art_node48_type_num) {
		an48 = (art_node48 *)pmemobj_direct(n);
		i = an48->keys[c];
		if (i) {
			return &(an48->children[i - 1]);
		}
	} else if (type_num == art_node256_type_num) {
		an256 = (art_node256 *)pmemobj_direct(n);
		if (!OID_IS_NULL(an256->children[c])) {
			return &(an256->children[c]);
		}
	} else {
		abort();
	}
	return &null_oid;
}

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
leaf_matches(art_leaf *n, const unsigned char *key, int key_len, int depth)
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
art_search(PMEMobjpool *pop, const unsigned char *key, int key_len)
{
	TOID(struct art_tree_root)t = POBJ_ROOT(pop, struct art_tree_root);
	PMEMoid *child;
	PMEMoid n = D_RO(t)->root;
	const art_node *n_an;
	int prefix_len;
	int depth = 0;
	int type_num;

	while (!OID_IS_NULL(n)) {
		/* Might be a leaf */
		type_num = pmemobj_type_num(n);
	    if (type_num == art_leaf_type_num) {
		    art_leaf *l = (art_leaf *)pmemobj_direct(n);
			/* Check if the expanded path matches */
			if (!leaf_matches(l, key, key_len, depth)) {
				return (void *)&(l->buffer[key_len]);
			}
			return (void *)NULL;
		}

		if (type_num == art_node4_type_num) {
			n_an = &((art_node4 *)pmemobj_direct(n))->n;
		} else if (type_num == art_node16_type_num) {
			n_an = &((art_node16 *)pmemobj_direct(n))->n;
		} else if (type_num == art_node48_type_num) {
			n_an = &((art_node48 *)pmemobj_direct(n))->n;
		} else if (type_num == art_node256_type_num) {
			n_an = &((art_node256 *)pmemobj_direct(n))->n;
		} else {
			return (void *)NULL;
		}

		/* Bail if the prefix does not match */
		if (n_an->partial_len) {
			prefix_len = check_prefix(n_an, key, key_len, depth);
			if (prefix_len !=
				    min(MAX_PREFIX_LEN, n_an->partial_len))
				return (void *)NULL;
			depth = depth + n_an->partial_len;
		}

		/* Recursively search */
		child = find_child(n, type_num, key[depth]);
		if (OID_IS_NULL(*child)) {
			n = OID_NULL;
		} else {
			n = *child;
		}
		depth++;
	}
	return (void *)NULL;
}

/*
 * Find the minimum leaf under a node
 */
static PMEMoid
minimum(PMEMoid n, uint64_t type_num)
{
	PMEMoid child;
	/* Handle base cases */
	if (OID_IS_NULL(n))
		return OID_NULL;

	if (type_num == art_leaf_type_num)
		return n;

	/* size_t type_num = pmemobj_type_num(n); */
	if (type_num == art_node4_type_num) {
		child = ((art_node4 *)pmemobj_direct(n))->children[0];
		return minimum(child, pmemobj_type_num(child));
	}
	if (type_num == art_node16_type_num) {
		child = ((art_node16 *)pmemobj_direct(n))->children[0];
		return minimum(child, pmemobj_type_num(child));
	}
	if (type_num == art_node48_type_num) {
		art_node48 *p;
		int idx;

		p = (art_node48 *)pmemobj_direct(n);
		idx = 0;
		while (!(p->keys[idx]))
			idx++;
		idx = p->keys[idx] - 1;
		assert(idx < 48);
		return minimum(p->children[idx],
		    pmemobj_type_num(p->children[idx]));
	}
	if (type_num == art_node256_type_num) {
		art_node256 *p;
		int idx;

		p = (art_node256 *)pmemobj_direct(n);
		idx = 0;
		while (!(OID_IS_NULL(p->children[idx])))
			idx++;
		return minimum(p->children[idx],
		    pmemobj_type_num(p->children[idx]));
	}
	if (type_num == art_leaf_type_num) {
		return n;
	}
	abort();
}

/*
 * Find the maximum leaf under a node
 */
static PMEMoid
maximum(PMEMoid n)
{
	int idx;

	/* Handle base cases */
	if (OID_IS_NULL(n))
		return OID_NULL;
	if (pmemobj_type_num(n) == art_leaf_type_num)
		return n;

	size_t type_num = pmemobj_type_num(n);
	if (type_num == art_node4_type_num) {
		art_node4 *an4;

		an4 = (art_node4 *)pmemobj_direct(n);
		return maximum(an4->children[an4->n.num_children - 1]);
	}
	if (type_num == art_node16_type_num) {
		art_node16 *an16;

		an16 = (art_node16 *)pmemobj_direct(n);
		return maximum(an16->children[an16->n.num_children - 1]);
	}
	if (type_num == art_node48_type_num) {
		art_node48 *an48;

		an48 = (art_node48 *)pmemobj_direct(n);
		idx = 255;
		while (!(an48->keys[idx]))
			idx--;
		idx = an48->keys[idx] - 1;
		assert((idx >= 0) && (idx < 48));
		return maximum(an48->children[idx]);
	}
	if (type_num == art_node256_type_num) {
		art_node256 *an256;

		an256 = (art_node256 *)pmemobj_direct(n);
		idx = 255;
		while (!(OID_IS_NULL(an256->children[idx])))
			idx--;
		return maximum(an256->children[idx]);
	}
	abort();
}

/*
 * Returns the minimum valued leaf
 */
PMEMoid
art_minimum(TOID(struct art_tree_root) t)
{
	return minimum(D_RO(t)->root, pmemobj_type_num(D_RO(t)->root));
}

/*
 * Returns the maximum valued leaf
 */
PMEMoid
art_maximum(TOID(struct art_tree_root) t)
{
	return maximum(D_RO(t)->root);
}

PMEMoid
make_leaf(PMEMobjpool *pop,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	PMEMoid newleaf;

	newleaf = alloc_node(pop, art_leaf_t, key_len + val_len);
	fill_leaf(pop, newleaf, key, key_len, value, val_len);

	return newleaf;
}

static int
longest_common_prefix(PMEMoid l1, PMEMoid l2, int depth)
{
	art_leaf *al1;
	art_leaf *al2;
	unsigned char *key_al1, *key_al2;
	int max_cmp;
	int idx = 0;

	if ((pmemobj_type_num(l1) != art_leaf_type_num) ||
	    (pmemobj_type_num(l2) != art_leaf_type_num)) {
		return 0;
	}

	al1 = pmemobj_direct(l1);
	al2 = pmemobj_direct(l2);
	key_al1 = &(al1->buffer[0]);
	key_al2 = &(al2->buffer[0]);

	max_cmp = min(al1->key_len, al2->key_len) - depth;
	for (idx = 0; idx < max_cmp; idx++) {
		if (key_al1[depth + idx] !=
		    key_al2[depth + idx])
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
add_child256(PMEMobjpool *pop, PMEMoid n, PMEMoid *ref,
	unsigned char c, PMEMoid child)
{
	art_node256 *np;

	(void) ref;

	pmemobj_tx_add_range(n, 0, sizeof(art_node256));
	np = (art_node256 *)pmemobj_direct(n);

	np->n.num_children++;
	np->children[c] = child;
}

static void
add_child48(PMEMobjpool *pop, PMEMoid n, PMEMoid *ref,
	unsigned char c, PMEMoid child)
{
	art_node48 *np;

	np = (art_node48 *)pmemobj_direct(n);
	if (np->n.num_children < 48) {
		int pos = 0;
		pmemobj_tx_add_range(n, 0, sizeof(art_node48));
		while (!(OID_IS_NULL(np->children[pos])))
			pos++;
		np->children[pos] = child;
		np->keys[c] = pos + 1;
		np->n.num_children++;
	} else {
		PMEMoid  newnode = alloc_node(pop, NODE256, 0);
		art_node256 *newnodep = pmemobj_direct(newnode);

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		for (int i = 0; i < 256; i++) {
			if (np->keys[i]) {
				newnodep->children[i] =
					np->children[np->keys[i]];
			}
		}
		copy_header(&(newnodep->n), &(np->n));
		*ref = newnode;
		pmemobj_tx_free(n);
		add_child256(pop, newnode, ref, c, child);
	}
}

static void
add_child16(PMEMobjpool *pop, PMEMoid n, PMEMoid *ref,
	unsigned char c, PMEMoid child)
{
	art_node16 *np;

	np = (art_node16 *)pmemobj_direct(n);
	if (np->n.num_children < 16) {
		__m128i cmp;

		pmemobj_tx_add_range(n, 0, sizeof(art_node16));

		/* Compare the key to all 16 stored keys */
		cmp = _mm_cmplt_epi8(_mm_set1_epi8(c),
			    _mm_loadu_si128((__m128i *)(np->keys)));

		/* Use a mask to ignore children that don't exist */
		unsigned mask = (1 << np->n.num_children) - 1;
		unsigned bitfield = _mm_movemask_epi8(cmp) & mask;

		/* Check if less than any */
		unsigned idx;
		if (bitfield) {
			idx = __builtin_ctz(bitfield);
			memmove(&(np->keys[idx + 1]),
			    &(np->keys[idx]),
			    np->n.num_children - idx);
			assert((idx + 1) < 16);
			PMEMOIDmove(&(np->children[idx + 1]),
			    &(np->children[idx]),
			    np->n.num_children - idx);
		} else {
			idx = np->n.num_children;
		}

		/* Set the child */
		np->keys[idx] = c;
		np->children[idx] = child;
		np->n.num_children++;

	} else {
		PMEMoid newnode = alloc_node(pop, NODE48, 0);
		art_node48 *newnodep = (art_node48 *)pmemobj_direct(newnode);

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		/* Copy the child pointers */
		for (int i = 0; i < np->n.num_children; i++) {
			newnodep->children[np->keys[i]] = np->children[i];
		}
		copy_header(&(newnodep->n), &(np->n));
		*ref = newnode;
		pmemobj_tx_free(n);
		add_child48(pop, newnode, ref, c, child);
	}
}

static void
add_child4(PMEMobjpool *pop, PMEMoid n, PMEMoid *ref,
	unsigned char c, PMEMoid child)
{
	art_node4 *np;

	np = (art_node4 *)pmemobj_direct(n);
	if (np->n.num_children < 4) {
		int idx;
		pmemobj_tx_add_range(n, 0, sizeof(art_node4));
		for (idx = 0; idx < np->n.num_children; idx++) {
			if (c < np->keys[idx]) break;
		}

		if (idx < np->n.num_children) {
			/* Shift to make room */
			memmove(np->keys + idx + 1, np->keys + idx,
			    np->n.num_children - idx);
			assert((idx + 1) < 4);
			PMEMOIDmove(&(np->children[idx + 1]),
			    &(np->children[idx]),
			np->n.num_children - idx);
		}

		/* Insert element */
		np->keys[idx] = c;
		np->children[idx] = child;
		np->n.num_children++;
	} else {
		PMEMoid newnode = alloc_node(pop, NODE16, 0);
		art_node16 *newnodep = (art_node16 *)pmemobj_direct(newnode);

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		/* Copy the child pointers and the key map */
		PMEMOIDcopy(&(newnodep->children[0]),
		    &(np->children[0]), np->n.num_children);
		memcpy(newnodep->keys, np->keys, np->n.num_children);
		copy_header(&(newnodep->n), &(np->n));
		*ref = newnode;
		pmemobj_tx_free(n);
		add_child16(pop, newnode, ref, c, child);
	}
}

static void
add_child(PMEMobjpool *pop, PMEMoid n, uint64_t type_num, PMEMoid *ref,
	unsigned char c, PMEMoid child)
{


	if (type_num == art_node4_type_num) {
		add_child4(pop, n, ref, c, child);
	} else if (type_num == art_node16_type_num) {
		add_child16(pop, n, ref, c, child);
	} else if (type_num == art_node48_type_num) {
		add_child48(pop, n, ref, c, child);
	} else if (type_num == art_node256_type_num) {
		add_child256(pop, n, ref, c, child);
	} else {
		abort();
	}
}

static int
prefix_mismatch(PMEMoid n, unsigned char *key, int key_len, int depth)
{
	const art_node *anp;
	int max_cmp;
	int idx;
	size_t type_num;

	type_num = pmemobj_type_num(n);
	if (type_num == art_node4_type_num) {
		anp = &(((art_node4 *)pmemobj_direct(n))->n);
	} else if (type_num == art_node16_type_num) {
		anp = &(((art_node16 *)pmemobj_direct(n))->n);
	} else if (type_num == art_node48_type_num) {
		anp = &(((art_node48 *)pmemobj_direct(n))->n);
	} else if (type_num == art_node256_type_num) {
		anp = &(((art_node256 *)pmemobj_direct(n))->n);
	} else {
		return 0;
	}
	max_cmp = min(min(MAX_PREFIX_LEN, anp->partial_len), key_len - depth);
	for (idx = 0; idx < max_cmp; idx++) {
		if (anp->partial[idx] != key[depth + idx]) {
			return idx;
		}
	}

	/* If the prefix is short we can avoid finding a leaf */
	if (anp->partial_len > MAX_PREFIX_LEN) {
		/* Prefix is longer than what we've checked, find a leaf */
		PMEMoid l = minimum(n, type_num);
		art_leaf *alp = pmemobj_direct(l);
		unsigned char *al_key = &(alp->buffer[0]);
		max_cmp = min(alp->key_len, key_len) - depth;
		for (; idx < max_cmp; idx++) {
			if (al_key[idx + depth] != key[depth + idx]) {
				return idx;
			}
		}
	}
	return idx;
}

static void *
recursive_insert(PMEMobjpool *pop, PMEMoid n, PMEMoid *ref,
	const unsigned char *key, int key_len,
	void *value, int val_len, int depth, int *old)
{
	size_t type_num;
	art_node *n_an;

	/* If we are at a NULL node, inject a leaf */
	if (OID_IS_NULL(n)) {
		*ref = make_leaf(pop, key, key_len, value, val_len);
		pmemobj_tx_add_range(*ref, 0, sizeof(PMEMoid));
		return NULL;
	}

	/* If we are at a leaf, we need to replace it with a node */
	type_num = pmemobj_type_num(n);
	if (type_num == art_leaf_type_num) {
		art_leaf *l = pmemobj_direct(n);

		/* Check if we are updating an existing value */
		if (!leaf_matches(l, key, key_len, depth)) {
			*old = 1;
			if (val_len > l->buffer_len - l->key_len) {
				n = pmemobj_tx_realloc(n,
				    sizeof(art_leaf) + key_len + val_len,
				    art_leaf_type_num);
				l = pmemobj_direct(n);
				l->buffer_len = key_len + val_len;
			}
			pmemobj_tx_add_range(n, 0,
			    sizeof(art_leaf) + l->buffer_len);
			/*
			 * XXX: should return the old value -
			 * but to do this we would need to allocate
			 * memory and copy into it
			 */
			memcpy(&(l->buffer[key_len]), value, val_len);
			l->val_len = val_len;
			/*
			 * XXX: should return the old value -
			 * but to do this we would need to
			 * allocate memory and copy into it
			 */
			return NULL;
		}

		/* New value, we must split the leaf into a node4 */
		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));
		PMEMoid newnode = alloc_node(pop, NODE4, 0);
		art_node4 *an4p = (art_node4 *)pmemobj_direct(newnode);

		/* Create a new leaf */

		PMEMoid l2oid =
		    make_leaf(pop, key, key_len, value, val_len);
		art_leaf *l2 = (art_leaf *)pmemobj_direct(l2oid);

		/* Determine longest prefix */
		int longest_prefix =
		    longest_common_prefix(n, l2oid, depth);
		an4p->n.partial_len = longest_prefix;
		memcpy(an4p->n.partial, key + depth,
		    min(MAX_PREFIX_LEN, longest_prefix));
		/* Add the leafs to the newnode node4 */
		*ref = newnode;
		add_child4(pop, newnode, ref,
		    l->buffer[depth + longest_prefix],
		    n);
		add_child4(pop, newnode, ref,
		    l2->buffer[depth + longest_prefix],
		    l2oid);
		return NULL;
	}

	/* Check if given node has a prefix */
	if (type_num == art_node4_type_num) {
		art_node4 *an4 = (art_node4 *)pmemobj_direct(n);
		n_an = &(an4->n);
	} else if (type_num == art_node16_type_num) {
		art_node16 *an16 = (art_node16 *)pmemobj_direct(n);
		n_an = &(an16->n);
	} else if (type_num == art_node48_type_num) {
		art_node48 *an48 = (art_node48 *)pmemobj_direct(n);
		n_an = &(an48->n);
	} else if (type_num == art_node256_type_num) {
		art_node256 *an256 = (art_node256 *)pmemobj_direct(n);
		n_an = &(an256->n);
	} else {
		abort();
	}

	if (n_an->partial_len) {
		/* Determine if the prefixes differ, since we need to split */
		int prefix_diff =
		    prefix_mismatch(n, (unsigned char *)key, key_len, depth);
		if ((uint32_t)prefix_diff >= n_an->partial_len) {
			depth += n_an->partial_len;
			goto RECURSE_SEARCH;
		}

		/* Create a new node */
		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));
		pmemobj_tx_add_range_direct(n_an, sizeof(art_node));
		PMEMoid new_oid = alloc_node(pop, NODE4, 0);
		art_node4 *new_node = pmemobj_direct(new_oid);

		*ref = new_oid;
		new_node->n.partial_len = prefix_diff;
		memcpy(new_node->n.partial, n_an->partial,
		    min(MAX_PREFIX_LEN, prefix_diff));

		/* Adjust the prefix of the old node */
		if (n_an->partial_len <= MAX_PREFIX_LEN) {
			add_child4(pop, new_oid, ref,
			    n_an->partial[prefix_diff], n);
			n_an->partial_len -= (prefix_diff + 1);
			memmove(n_an->partial,
			    n_an->partial + prefix_diff + 1,
			    min(MAX_PREFIX_LEN, n_an->partial_len));
		} else {
			unsigned char *dst;
			const unsigned char *src;
			size_t len;

			n_an->partial_len -= (prefix_diff + 1);
			PMEMoid l_oid = minimum(n, type_num);
			art_leaf *l = (art_leaf *)pmemobj_direct(l_oid);
			add_child4(pop, new_oid, ref,
			    l->buffer[depth + prefix_diff],
			    n);
			dst = n_an->partial;
			src = &(l->buffer[depth + prefix_diff + 1 ]);
			len = min(MAX_PREFIX_LEN, n_an->partial_len);

			memcpy(dst, src, len);
		}

		/* Insert the new leaf */
		PMEMoid l_oid = make_leaf(pop, key, key_len, value, val_len);
		add_child4(pop, new_oid, ref, key[depth + prefix_diff], l_oid);
		return NULL;
	}

RECURSE_SEARCH:;

	/* Find a child to recurse to */
	PMEMoid *child = find_child(n, type_num, key[depth]);
	if (!OID_IS_NULL(*child)) {
		return recursive_insert(pop, *child, child,
			    key, key_len, value, val_len, depth + 1, old);
	}

	/* No child, node goes within us */
	PMEMoid l_oid = make_leaf(pop, key, key_len, value, val_len);
	add_child(pop, n, type_num, ref, key[depth], l_oid);

	return NULL;
}

/*
 * Returns the size of the ART tree
 */
uint64_t
art_size(PMEMobjpool *pop)
{
	TOID(struct art_tree_root) root;
	root = POBJ_ROOT(pop, struct art_tree_root);
	return D_RO(root)->size;
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
art_insert(PMEMobjpool *pop,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	int old_val = 0;
	void *old = NULL;
	TOID(struct art_tree_root) root;

	TX_BEGIN(pop) {
		root = POBJ_ROOT(pop, struct art_tree_root);
		TX_ADD(root);

		old = recursive_insert(pop, D_RO(root)->root,
			    &(D_RW(root)->root),
			    (const unsigned char *)key, key_len,
			    value, val_len, 0, &old_val);
		if (!old_val)
			D_RW(root)->size++;
	} TX_ONABORT {
		abort();
	} TX_END

	return old;
}

static void
remove_child256(PMEMobjpool *pop,
	PMEMoid n, PMEMoid *ref, unsigned char c)
{
	art_node256 *n256 = (art_node256 *)pmemobj_direct(n);

	pmemobj_tx_add_range(n, 0, sizeof(art_node256));

	n256->children[c] = OID_NULL;
	n256->n.num_children--;

	/*
	 * Resize to a node48 on underflow, not immediately to prevent
	 * trashing if we sit on the 48/49 boundary
	 */
	if (n256->n.num_children == 37) {
		PMEMoid newnode_n48oid;
		art_node48 *new_n48;

		newnode_n48oid = alloc_node(pop, NODE48, 0);
		new_n48 = (art_node48 *)pmemobj_direct(newnode_n48oid);

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		*ref = newnode_n48oid;
		copy_header(&(new_n48->n), &(n256->n));

		int pos = 0;
		for (int i = 0; i < 256; i++) {
			if (!OID_IS_NULL(n256->children[i])) {
				assert(pos < 48);
				new_n48->children[pos] =
				    n256->children[i];
				new_n48->keys[i] = pos + 1;
				pos++;
			}
		}
		pmemobj_tx_free(n);
	}
}

static void
remove_child48(PMEMobjpool *pop,
	PMEMoid n, PMEMoid *ref, unsigned char c)
{
	art_node48 *n48 = (art_node48 *)pmemobj_direct(n);
	int pos = n48->keys[c];

	pmemobj_tx_add_range(n, 0, sizeof(art_node48));

	n48->keys[c] = 0;
	n48->children[pos - 1] = OID_NULL;
	n48->n.num_children--;

	if (n48->n.num_children == 12) {
		PMEMoid newnode_n16oid;
		art_node16 *new_n16;

		newnode_n16oid = alloc_node(pop, NODE16, 0);
		new_n16 = (art_node16 *)pmemobj_direct(newnode_n16oid);

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		*ref = newnode_n16oid;
		copy_header(&(new_n16->n), &(n48->n));

		int child = 0;
		for (int i = 0; i < 256; i++) {
			pos = n48->keys[i];
			if (pos) {
				assert(child < 16);
				new_n16->keys[child] = i;
				new_n16->children[child] =
				    n48->children[pos - 1];
				child++;
			}
		}
		pmemobj_tx_free(n);
	}
}

static void
remove_child16(PMEMobjpool *pop,
	PMEMoid n, PMEMoid *ref, PMEMoid *l)
{
	art_node16 *n16;
	int pos;
	uint8_t num_children;

	n16 = (art_node16 *)pmemobj_direct(n);
	pos = ((uint64_t)l - (uint64_t)&(n16->children[0])) / sizeof(PMEMoid);
	num_children = n16->n.num_children;

	pmemobj_tx_add_range(n, 0, sizeof(art_node16));

	memmove(n16->keys + pos, n16->keys + pos + 1,
	    num_children - 1 - pos);
	memmove(n16->children + pos,
	    n16->children + pos + 1,
	    (num_children - 1 - pos) * sizeof(PMEMoid));
	n16->n.num_children--;

	if (--num_children == 3) {
		PMEMoid newnode_n4oid;
		art_node4 *new_n4;

		newnode_n4oid  = alloc_node(pop, NODE4, 0);
		new_n4 = (art_node4 *)pmemobj_direct(newnode_n4oid);

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		*ref = newnode_n4oid;
		copy_header(&(new_n4->n), &(n16->n));
		memcpy(new_n4->keys, n16->keys, 4);
		memcpy(new_n4->children,
		    n16->children, 4 * sizeof(PMEMoid));
		pmemobj_tx_free(n);
	}
}

static void
remove_child4(PMEMobjpool *pop,
	PMEMoid n, PMEMoid *ref, PMEMoid *l)
{
	int pos;
	uint8_t *num_children;
	art_node4 *an4;

	an4 = (art_node4 *)pmemobj_direct(n);
	pos = ((uint64_t)l - (uint64_t)&(an4->children[0])) / sizeof(PMEMoid);
	num_children = &(an4->n.num_children);

	pmemobj_tx_add_range(n, 0, sizeof(art_node4));

	memmove(an4->keys + pos, an4->keys + pos + 1,
	    *num_children - 1 - pos);
	memmove(an4->children + pos, an4->children + pos + 1,
	    (*num_children - 1 - pos) * sizeof(PMEMoid));
	(*num_children)--;

	/* Remove nodes with only a single child */
	if (*num_children == 1) {
		PMEMoid childoid = an4->children[0];

		pmemobj_tx_add_range_direct(ref, sizeof(PMEMoid));

		if (pmemobj_type_num(childoid) != art_leaf_type_num) {
			art_node *child;
			child = ((art_node *)pmemobj_direct(childoid));
			/* Concatenate the prefixes */
			int prefix = an4->n.partial_len;
			if (prefix < MAX_PREFIX_LEN) {
				an4->n.partial[prefix] =
				    an4->keys[0];
				prefix++;
			}
			if (prefix < MAX_PREFIX_LEN) {
				int sub_prefix = min(child->partial_len,
				    MAX_PREFIX_LEN - prefix);
				memcpy(an4->n.partial + prefix,
				    child->partial, sub_prefix);
				prefix += sub_prefix;
			}

			/* Store the prefix in the child */
			memcpy(child->partial,
			    an4->n.partial, min(prefix, MAX_PREFIX_LEN));
			child->partial_len += an4->n.partial_len + 1;
		}
		*ref = childoid;
		pmemobj_tx_free(n);
	}
}

static void
remove_child(PMEMobjpool *pop,
	PMEMoid n, PMEMoid *ref,
	unsigned char c, PMEMoid *l)
{
	uint64_t type_num;

	type_num = pmemobj_type_num(n);
	if (type_num == art_node4_type_num) {
		return remove_child4(pop,   n, ref, l);
	} else if (type_num == art_node16_type_num) {
		return remove_child16(pop,  n,  ref, l);
	} else if (type_num == art_node48_type_num) {
		return remove_child48(pop,  n,  ref, c);
	} else if (type_num == art_node256_type_num) {
		return remove_child256(pop, n, ref, c);
	} else {
		abort();
	}
}

PMEMoid
recursive_delete(PMEMobjpool *pop,
	PMEMoid n, PMEMoid *ref,
	const unsigned char *key, int key_len, int depth)
{
	const art_node *n_an;
	uint64_t type_num;

	/* Search terminated */
	if (OID_IS_NULL(n))
		return OID_NULL;

	type_num = pmemobj_type_num(n);
	/* Handle hitting a leaf node */
	if (type_num == art_leaf_type_num) {
		art_leaf *l = (art_leaf *)pmemobj_direct(n);
		if (!leaf_matches(l, key, key_len, depth)) {
			*ref = OID_NULL;
			return n;
		}
		return OID_NULL;
	}

	/* get art_node component */
	if (type_num == art_node4_type_num) {
		n_an = &(((art_node4 *)pmemobj_direct(n))->n);
	} else if (type_num == art_node16_type_num) {
		n_an = &(((art_node16 *)pmemobj_direct(n))->n);
	} else if (type_num == art_node48_type_num) {
		n_an = &(((art_node48 *)pmemobj_direct(n))->n);
	} else if (type_num == art_node256_type_num) {
		n_an = &(((art_node256 *)pmemobj_direct(n))->n);
	} else {
		abort();
	}

	/* Bail if the prefix does not match */
	if (n_an->partial_len) {
		int prefix_len = check_prefix(n_an, key, key_len, depth);
		if (prefix_len != min(MAX_PREFIX_LEN, n_an->partial_len)) {
			return OID_NULL;
		}
		depth = depth + n_an->partial_len;
	}

	/* Find child node */
	PMEMoid *child = find_child(n, type_num, key[depth]);
	if (OID_IS_NULL(*child))
		return OID_NULL;

	/* If the child is leaf, delete from this node */
	if (pmemobj_type_num(*child) == art_leaf_type_num) {
		art_leaf *l = (art_leaf *)pmemobj_direct(*child);
		if (!leaf_matches(l, key, key_len, depth)) {
			remove_child(pop, n, ref, key[depth], child);
			return *child;
		}
		return OID_NULL;
	} else {
		/* Recurse */
		return recursive_delete(pop, *child, child,
			    (const unsigned char *)key, key_len, depth + 1);
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
art_delete(PMEMobjpool *pop,
	const unsigned char *key, int key_len)
{
	TOID(struct art_tree_root) root;
	PMEMoid loid;
	art_leaf *l;
	void *retval;

	retval = NULL;
	root = POBJ_ROOT(pop, struct art_tree_root);

	TX_BEGIN(pop) {
		TX_ADD(root);
		loid = recursive_delete(pop, D_RO(root)->root,
		    &D_RW(root)->root, key, key_len, 0);
		if (!OID_IS_NULL(loid)) {
			l = (art_leaf *)pmemobj_direct(loid);
			D_RW(root)->size--;
			retval = (void *)malloc(l->key_len);
			if (retval != NULL) {
				(void) memcpy(retval,
				    (void *)&(l->buffer[l->key_len]),
				    l->key_len);
			}
			pmemobj_tx_free(loid);
		}
	} TX_ONABORT {
		abort();
	} TX_END

	return retval;
}

/*
 * Recursively iterates over the tree
 */
static int
recursive_iter2(PMEMoid n, art_callback cb, void *data)
{
	uint64_t type_num;
	const art_node    *n_an;
	art_node4 *an4;
	art_node16 *an16;
	art_node48 *an48;
	art_node256 *an256;
	art_leaf *l;
	unsigned char *key;
	unsigned char *value;
	cb_data cbd;

	/* Handle base cases */
	if (OID_IS_NULL(n)) {
		return 0;
	}

	cbd.node = n;
	cbd.child_idx = -1;
	type_num = pmemobj_type_num(n);
	if (type_num == art_leaf_type_num) {
		l = (art_leaf *)pmemobj_direct(n);
		key = &(l->buffer[0]);
		value = &(l->buffer[l->key_len]);
		return cb(&cbd, key, l->key_len, value, l->val_len);
	}

	int idx, res;
	if (type_num == art_node4_type_num) {
		an4 = (art_node4 *)pmemobj_direct(n);
		n_an = &(an4->n);
		for (int i = 0; i < n_an->num_children; i++) {
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(an4->children[i], cb, data);
			if (res)
				return res;
		}
	} else if (type_num == art_node16_type_num) {
		an16 = (art_node16 *)pmemobj_direct(n);
		n_an = &(an16->n);
		for (int i = 0; i < n_an->num_children; i++) {
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(an16->children[i], cb, data);
			if (res)
				return res;
		}
	} else if (type_num == art_node48_type_num) {
		an48 = (art_node48 *)pmemobj_direct(n);
		for (int i = 0; i < 256; i++) {
			idx = an48->keys[i];
			if (!idx)
				continue;

			cbd.child_idx = idx - 1;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(an48->children[idx - 1],
				    cb, data);
			if (res)
				return res;
		}
	} else if (type_num == art_node256_type_num) {
		an256 = (art_node256 *)pmemobj_direct(n);

		for (int i = 0; i < 256; i++) {
			if (OID_IS_NULL(an256->children[i]))
				continue;
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter2(an256->children[i],
				    cb, data);
			if (res)
				return res;
		}
	} else {
		abort();
	}
	return 0;
}

/*
 * Recursively iterates over the tree
 */
static int
recursive_iter(PMEMoid n, art_callback cb, void *data)
{
	uint64_t type_num;
	const art_node    *n_an;
	art_node4 *an4;
	art_node16 *an16;
	art_node48 *an48;
	art_node256 *an256;
	art_leaf *l;
	unsigned char *key;
	unsigned char *value;
	cb_data cbd;

	/* Handle base cases */
	if (OID_IS_NULL(n)) {
		return 0;
	}

	cbd.node = n;
	cbd.child_idx = -1;
	type_num = pmemobj_type_num(n);
	if (type_num == art_leaf_type_num) {
		l = (art_leaf *)pmemobj_direct(n);
		key = &(l->buffer[0]);
		value = &(l->buffer[l->key_len]);
		return cb(&cbd, key, l->key_len, value, l->val_len);
	}

	int idx, res;
	if (type_num == art_node4_type_num) {
		an4 = (art_node4 *)pmemobj_direct(n);
		n_an = &(an4->n);
		for (int i = 0; i < n_an->num_children; i++) {
			res = recursive_iter(an4->children[i], cb, data);
			if (res)
				return res;
		}
	} else if (type_num == art_node16_type_num) {
		an16 = (art_node16 *)pmemobj_direct(n);
		n_an = &(an16->n);
		for (int i = 0; i < n_an->num_children; i++) {
			res = recursive_iter(an16->children[i], cb, data);
			if (res)
				return res;
		}
	} else if (type_num == art_node48_type_num) {
		an48 = (art_node48 *)pmemobj_direct(n);
		for (int i = 0; i < 256; i++) {
			idx = an48->keys[i];
			if (!idx)
				continue;

			res = recursive_iter(an48->children[idx - 1],
				    cb, data);
			if (res)
				return res;
		}
	} else if (type_num == art_node256_type_num) {
		an256 = (art_node256 *)pmemobj_direct(n);

		for (int i = 0; i < 256; i++) {
			if (OID_IS_NULL(an256->children[i]))
				continue;
			res = recursive_iter(an256->children[i],
				    cb, data);
			if (res)
				return res;
		}
	} else {
		abort();
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
art_iter(PMEMobjpool *pop, art_callback cb, void *data)
{
	TOID(struct art_tree_root) t = POBJ_ROOT(pop, struct art_tree_root);
	return recursive_iter(D_RO(t)->root, cb, data);
}

int
art_iter2(PMEMobjpool *pop, art_callback cb, void *data)
{
	TOID(struct art_tree_root) t = POBJ_ROOT(pop, struct art_tree_root);
	return recursive_iter2(D_RO(t)->root, cb, data);
}

#ifdef LIBART_ITER_PREFIX
/*
 * Checks if a leaf prefix matches
 * @return 0 on success.
 */
static int
leaf_prefix_matches(TOID(art_leaf) n,
	const unsigned char *prefix, int prefix_len)
{
	/* Fail if the key length is too short */
	if (D_RO(D_RO(n)->key)->len < (uint32_t)prefix_len)
		return 1;

	/* Compare the keys */
	return memcmp(D_RO(D_RO(n)->key)->s, prefix, prefix_len);
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
art_iter_prefix(art_tree *t,
	const unsigned char *key, int key_len, art_callback cb, void *data)
{
	art_node **child;
	art_node *n = t->root;
	int prefix_len, depth = 0;
	while (n) {
		/* Might be a leaf */
		if (IS_LEAF(n)) {
			n = LEAF_RAW(n);
			/* Check if the expanded path matches */
			if (!leaf_prefix_matches((art_leaf *)n, key, key_len)) {
				art_leaf *l = (art_leaf *)n;
				return cb(data,
					    (const unsigned char *)l->key,
					    l->key_len, l->value);
			}
			return 0;
		}

		/*
		 * If the depth matches the prefix,
		 * we need to handle this node
		 */
		if (depth == key_len) {
			art_leaf *l = minimum(n);
			if (!leaf_prefix_matches(l, key, key_len))
				return recursive_iter(n, cb, data);
			return 0;
		}

		/* Bail if the prefix does not match */
		if (n->partial_len) {
			prefix_len = prefix_mismatch(n, key, key_len, depth);

			/* If there is no match, search is terminated */
			if (!prefix_len)
				return 0;

			/* If we've matched the prefix, iterate on this node */
			else if (depth + prefix_len == key_len) {
				return recursive_iter(n, cb, data);
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
#endif /* } LIBART_ITER_PREFIX */

int
fill_leaf(PMEMobjpool *pop, PMEMoid al,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	art_leaf *alp;

	alp = (art_leaf *)pmemobj_direct(al);

	assert(alp->buffer_len >= (key_len + val_len));

	alp->key_len = key_len;
	alp->val_len = val_len;
	pmemobj_tx_add_range_direct((void *)&(alp->buffer[0]),
	    key_len + val_len);
	memcpy((void *)&(alp->buffer[0]),
	    (void *)key, (size_t)key_len);
	memcpy((void *)&(alp->buffer[key_len]),
	    (void *)value, (size_t)val_len);

	return 0;
}
