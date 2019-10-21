/*
 * Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
 * Copyright 2012, Armon Dadgar. All rights reserved.
 * Copyright 2016-2019, Intel Corporation
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
 *                  Andreas.Bluemle.external@ts.fujitsu.com
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

TOID(var_string) null_var_string;
TOID(art_leaf)   null_art_leaf;
TOID(art_node_u) null_art_node_u;

int art_tree_init(PMEMobjpool *pop, int *newpool);
TOID(art_node_u) make_leaf(PMEMobjpool *pop, const unsigned char *key,
			    int key_len, void *value, int val_len);
int fill_leaf(PMEMobjpool *pop, TOID(art_leaf) al, const unsigned char *key,
			    int key_len, void *value, int val_len);
TOID(art_node_u) alloc_node(PMEMobjpool *pop, art_node_type node_type);

TOID(var_string) art_insert(PMEMobjpool *pop, const unsigned char *key,
			    int key_len, void *value, int val_len);
TOID(var_string) art_delete(PMEMobjpool *pop, const unsigned char *key,
			    int key_len);
static TOID(var_string) recursive_insert(PMEMobjpool *pop,
			    TOID(art_node_u) n, TOID(art_node_u) *ref,
			    const unsigned char *key, int key_len,
			    void *value, int val_len, int depth, int *old_val);
static TOID(art_leaf) recursive_delete(PMEMobjpool *pop,
			    TOID(art_node_u) n, TOID(art_node_u) *ref,
			    const unsigned char *key, int key_len, int depth);
static int leaf_matches(TOID(art_leaf) n, const unsigned char *key,
			    int key_len, int depth);
static int longest_common_prefix(TOID(art_leaf) l1, TOID(art_leaf) l2,
			    int depth);
static int prefix_mismatch(TOID(art_node_u) n, unsigned char *key,
			    int key_len, int depth);
#ifdef LIBART_ITER_PREFIX
static int leaf_prefix_matches(TOID(art_leaf) n,
			    const unsigned char *prefix, int prefix_len);
#endif

static TOID(art_leaf) minimum(TOID(art_node_u) n_u);
static TOID(art_leaf) maximum(TOID(art_node_u) n_u);
static void copy_header(art_node *dest, art_node *src);

static void add_child(PMEMobjpool *pop, TOID(art_node_u) n,
			    TOID(art_node_u) *ref, unsigned char c,
			    TOID(art_node_u) child);
static void add_child4(PMEMobjpool *pop, TOID(art_node4) n,
			    TOID(art_node_u) *ref, unsigned char c,
			    TOID(art_node_u) child);
static void add_child16(PMEMobjpool *pop, TOID(art_node16) n,
			    TOID(art_node_u) *ref, unsigned char c,
			    TOID(art_node_u) child);
static void add_child48(PMEMobjpool *pop, TOID(art_node48) n,
			    TOID(art_node_u) *ref, unsigned char c,
			    TOID(art_node_u) child);
static void add_child256(PMEMobjpool *pop, TOID(art_node256) n,
			    TOID(art_node_u) *ref, unsigned char c,
			    TOID(art_node_u) child);
static void remove_child(PMEMobjpool *pop, TOID(art_node_u) n,
			    TOID(art_node_u) *ref, unsigned char c,
			    TOID(art_node_u) *l);
static void remove_child4(PMEMobjpool *pop, TOID(art_node4) n,
			    TOID(art_node_u) *ref, TOID(art_node_u) *l);
static void remove_child16(PMEMobjpool *pop, TOID(art_node16) n,
			    TOID(art_node_u) *ref, TOID(art_node_u) *l);
static void remove_child48(PMEMobjpool *pop, TOID(art_node48) n,
			    TOID(art_node_u) *ref, unsigned char c);
static void remove_child256(PMEMobjpool *pop, TOID(art_node256) n,
			    TOID(art_node_u) *ref, unsigned char c);

static TOID(art_node_u)* find_child(TOID(art_node_u) n, unsigned char c);
static int check_prefix(const art_node *n, const unsigned char *key,
			    int key_len, int depth);
static int leaf_matches(TOID(art_leaf) n, const unsigned char *key,
			    int key_len, int depth);

TOID(art_leaf) art_minimum(TOID(struct art_tree_root) t);
TOID(art_leaf) art_maximum(TOID(struct art_tree_root) t);

#if 0
static void destroy_node(TOID(art_node_u) n_u);
#endif
int art_iter(PMEMobjpool *pop, art_callback cb, void *data);

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

TOID(art_node_u)
alloc_node(PMEMobjpool *pop, art_node_type node_type)
{
	TOID(art_node_u)  node;
	TOID(art_node4)   an4;
	TOID(art_node16)  an16;
	TOID(art_node48)  an48;
	TOID(art_node256) an256;
	TOID(art_leaf)    al;

	node = TX_ZNEW(art_node_u);
	D_RW(node)->art_node_type = (uint8_t)node_type;
	switch (node_type) {
	case NODE4:
		an4 = TX_ZNEW(art_node4);
		D_RW(node)->u.an4 = an4;
		break;
	case NODE16:
		an16 = TX_ZNEW(art_node16);
		D_RW(node)->u.an16 = an16;
		break;
	case NODE48:
		an48 = TX_ZNEW(art_node48);
		D_RW(node)->u.an48 = an48;
		break;
	case NODE256:
		an256 = TX_ZNEW(art_node256);
		D_RW(node)->u.an256 = an256;
		break;
	case art_leaf_t:
		al = TX_ZNEW(art_leaf);
		D_RW(node)->u.al = al;
		break;
	default:
		/* invalid node type */
		D_RW(node)->art_node_type = (uint8_t)art_node_types;
		break;
	}

	return node;
}

int
art_tree_init(PMEMobjpool *pop, int *newpool)
{
	int errors = 0;
	TOID(struct art_tree_root) root;

	if (pop == NULL) {
		errors++;
	}

	null_var_string.oid = OID_NULL;
	null_art_leaf.oid = OID_NULL;
	null_art_node_u.oid = OID_NULL;

	if (!errors) {
		TX_BEGIN(pop) {
			root = POBJ_ROOT(pop, struct art_tree_root);
			if (*newpool) {
				TX_ADD(root);
				D_RW(root)->root.oid = OID_NULL;
				D_RW(root)->size = 0;
				*newpool = 0;
			}
		} TX_END
	}

	return errors;
}

#if 0
// Recursively destroys the tree
static void
destroy_node(TOID(art_node_u) n_u)
{
	// Break if null
	if (TOID_IS_NULL(n_u))
		return;

	// Special case leafs
	if (IS_LEAF(D_RO(n_u))) {
		TX_FREE(n_u);
		return;
	}

	// Handle each node type
	int i;
	TOID(art_node4)   an4;
	TOID(art_node16)  an16;
	TOID(art_node48)  an48;
	TOID(art_node256) an256;

	switch (D_RO(n_u)->art_node_type) {
	case NODE4:
		an4 = D_RO(n_u)->u.an4;
		for (i = 0; i < D_RO(an4)->n.num_children; i++) {
			destroy_node(D_RW(an4)->children[i]);
		}
		break;

	case NODE16:
		an16 = D_RO(n_u)->u.an16;
		for (i = 0; i < D_RO(an16)->n.num_children; i++) {
			destroy_node(D_RW(an16)->children[i]);
		}
		break;

	case NODE48:
		an48 = D_RO(n_u)->u.an48;
		for (i = 0; i < D_RO(an48)->n.num_children; i++) {
			destroy_node(D_RW(an48)->children[i]);
		}
		break;

	case NODE256:
		an256 = D_RO(n_u)->u.an256;
		for (i = 0; i < D_RO(an256)->n.num_children; i++) {
			if (!(TOID_IS_NULL(D_RO(an256)->children[i]))) {
				destroy_node(D_RW(an256)->children[i]);
			}
		}
		break;

	default:
		abort();
	}

	// Free ourself on the way up
	TX_FREE(n_u);
}

/*
 * Destroys an ART tree
 * @return 0 on success.
 */
static int
art_tree_destroy(TOID(struct art_tree_root) t)
{
	destroy_node(D_RO(t)->root);
	return 0;
}
#endif

static TOID(art_node_u)*
find_child(TOID(art_node_u) n, unsigned char c)
{
	int i;
	int mask;
	int bitfield;
	TOID(art_node4)   an4;
	TOID(art_node16)  an16;
	TOID(art_node48)  an48;
	TOID(art_node256) an256;

	switch (D_RO(n)->art_node_type) {
	case NODE4:
		an4 = D_RO(n)->u.an4;
		for (i = 0; i < D_RO(an4)->n.num_children; i++) {
			if (D_RO(an4)->keys[i] == c) {
				return &(D_RW(an4)->children[i]);
			}
		}
		break;

	case NODE16: {
		__m128i cmp;
		an16 = D_RO(n)->u.an16;

		// Compare the key to all 16 stored keys
		cmp = _mm_cmpeq_epi8(_mm_set1_epi8(c),
			    _mm_loadu_si128((__m128i *)D_RO(an16)->keys));

		// Use a mask to ignore children that don't exist
		mask = (1 << D_RO(an16)->n.num_children) - 1;
		bitfield = _mm_movemask_epi8(cmp) & mask;

		/*
		 * If we have a match (any bit set) then we can
		 * return the pointer match using ctz to get the index.
		 */
		if (bitfield) {
			return &(D_RW(an16)->children[__builtin_ctz(bitfield)]);
		}
		break;
	}

	case NODE48:
		an48 = D_RO(n)->u.an48;
		i = D_RO(an48)->keys[c];
		if (i) {
			return &(D_RW(an48)->children[i - 1]);
		}
		break;

	case NODE256:
		an256 = D_RO(n)->u.an256;
		if (!TOID_IS_NULL(D_RO(an256)->children[c])) {
			return &(D_RW(an256)->children[c]);
		}
		break;

	default:
		abort();
	}
	return &null_art_node_u;
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
leaf_matches(TOID(art_leaf) n, const unsigned char *key, int key_len, int depth)
{
	(void) depth;
	// Fail if the key lengths are different
	if (D_RO(D_RO(n)->key)->len != (uint32_t)key_len)
		return 1;

	// Compare the keys starting at the depth
	return memcmp(D_RO(D_RO(n)->key)->s, key, key_len);
}

/*
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
TOID(var_string)
art_search(PMEMobjpool *pop, const unsigned char *key, int key_len)
{
	TOID(struct art_tree_root)t = POBJ_ROOT(pop, struct art_tree_root);
	TOID(art_node_u) *child;
	TOID(art_node_u) n = D_RO(t)->root;
	const art_node *n_an;
	int prefix_len;
	int depth = 0;

	while (!TOID_IS_NULL(n)) {
		// Might be a leaf
		if (IS_LEAF(D_RO(n))) {
			// n = LEAF_RAW(n);
			// Check if the expanded path matches
			if (!leaf_matches(D_RO(n)->u.al, key, key_len, depth)) {
				return (D_RO(D_RO(n)->u.al))->value;
			}
			return null_var_string;
		}

		switch (D_RO(n)->art_node_type) {
		case NODE4:    n_an = &(D_RO(D_RO(n)->u.an4)->n);   break;
		case NODE16:   n_an = &(D_RO(D_RO(n)->u.an16)->n);  break;
		case NODE48:   n_an = &(D_RO(D_RO(n)->u.an48)->n);  break;
		case NODE256:  n_an = &(D_RO(D_RO(n)->u.an256)->n); break;
		default:
			return null_var_string;
		}

		// Bail if the prefix does not match
		if (n_an->partial_len) {
			prefix_len = check_prefix(n_an, key, key_len, depth);
			if (prefix_len !=
				    min(MAX_PREFIX_LEN, n_an->partial_len))
				return null_var_string;
			depth = depth + n_an->partial_len;
		}

		// Recursively search
		child = find_child(n, key[depth]);
		if (TOID_IS_NULL(*child)) {
			n.oid = OID_NULL;
		} else {
			n = *child;
		}
		depth++;
	}
	return null_var_string;
}

// Find the minimum leaf under a node
static TOID(art_leaf)
minimum(TOID(art_node_u) n_u)
{
	TOID(art_node4)   an4;
	TOID(art_node16)  an16;
	TOID(art_node48)  an48;
	TOID(art_node256) an256;

	// Handle base cases
	if (TOID_IS_NULL(n_u))
		return null_art_leaf;
	if (IS_LEAF(D_RO(n_u)))
		return D_RO(n_u)->u.al;

	int idx;
	switch (D_RO(n_u)->art_node_type) {
	case NODE4:
		an4 = D_RO(n_u)->u.an4;
		return minimum(D_RO(an4)->children[0]);
	case NODE16:
		an16 = D_RO(n_u)->u.an16;
		return minimum(D_RO(an16)->children[0]);
	case NODE48:
		an48 = D_RO(n_u)->u.an48;
		idx = 0;
		while (!(D_RO(an48)->keys[idx]))
			idx++;
		idx = D_RO(an48)->keys[idx] - 1;
		assert(idx < 48);
		return minimum(D_RO(an48)->children[idx]);
	case NODE256:
		an256 = D_RO(n_u)->u.an256;
		idx = 0;
		while (!(TOID_IS_NULL(D_RO(an256)->children[idx])))
			idx++;
		return minimum(D_RO(an256)->children[idx]);
	default:
		abort();
	}
}

// Find the maximum leaf under a node
static TOID(art_leaf)
maximum(TOID(art_node_u) n_u)
{
	TOID(art_node4)   an4;
	TOID(art_node16)  an16;
	TOID(art_node48)  an48;
	TOID(art_node256) an256;
	const art_node *n_an;

	// Handle base cases
	if (TOID_IS_NULL(n_u))
		return null_art_leaf;
	if (IS_LEAF(D_RO(n_u)))
		return D_RO(n_u)->u.al;

	int idx;
	switch (D_RO(n_u)->art_node_type) {
	case NODE4:
		an4 = D_RO(n_u)->u.an4;
		n_an = &(D_RO(an4)->n);
		return maximum(D_RO(an4)->children[n_an->num_children - 1]);
	case NODE16:
		an16 = D_RO(n_u)->u.an16;
		n_an = &(D_RO(an16)->n);
		return maximum(D_RO(an16)->children[n_an->num_children - 1]);
	case NODE48:
		an48 = D_RO(n_u)->u.an48;
		idx = 255;
		while (!(D_RO(an48)->keys[idx]))
			idx--;
		idx = D_RO(an48)->keys[idx] - 1;
		assert((idx >= 0) && (idx < 48));
		return maximum(D_RO(an48)->children[idx]);
	case NODE256:
		an256 = D_RO(n_u)->u.an256;
		idx = 255;
		while (!(TOID_IS_NULL(D_RO(an256)->children[idx])))
			idx--;
		return maximum(D_RO(an256)->children[idx]);
	default:
		abort();
	}
}

/*
 * Returns the minimum valued leaf
 */
TOID(art_leaf)
art_minimum(TOID(struct art_tree_root) t)
{
	return minimum(D_RO(t)->root);
}

/*
 * Returns the maximum valued leaf
 */
TOID(art_leaf)
art_maximum(TOID(struct art_tree_root) t)
{
	return maximum(D_RO(t)->root);
}

TOID(art_node_u)
make_leaf(PMEMobjpool *pop,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	TOID(art_node_u)newleaf;

	newleaf = alloc_node(pop, art_leaf_t);
	fill_leaf(pop, D_RW(newleaf)->u.al, key, key_len, value, val_len);

	return newleaf;
}

static int
longest_common_prefix(TOID(art_leaf) l1, TOID(art_leaf) l2, int depth)
{
	TOID(var_string) l1_key = D_RO(l1)->key;
	TOID(var_string) l2_key = D_RO(l2)->key;
	int max_cmp;
	int idx;

	max_cmp = min(D_RO(l1_key)->len, D_RO(l2_key)->len) - depth;
	for (idx = 0; idx < max_cmp; idx++) {
		if (D_RO(l1_key)->s[depth + idx] !=
		    D_RO(l2_key)->s[depth + idx])
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
add_child256(PMEMobjpool *pop, TOID(art_node256) n, TOID(art_node_u) *ref,
	unsigned char c, TOID(art_node_u) child)
{
	art_node *n_an;

	(void) ref;

	TX_ADD(n);

	n_an = &(D_RW(n)->n);
	n_an->num_children++;
	D_RW(n)->children[c] = child;
}

static void
add_child48(PMEMobjpool *pop, TOID(art_node48) n, TOID(art_node_u) *ref,
	unsigned char c, TOID(art_node_u) child)
{
	art_node *n_an;

	n_an = &(D_RW(n)->n);
	if (n_an->num_children < 48) {
		int pos = 0;
		TX_ADD(n);
		while (!(TOID_IS_NULL(D_RO(n)->children[pos])))
			pos++;
		D_RW(n)->children[pos] = child;
		D_RW(n)->keys[c] = pos + 1;
		n_an->num_children++;
	} else {
		TOID(art_node_u)  newnode_u = alloc_node(pop, NODE256);
		TOID(art_node256) newnode = D_RO(newnode_u)->u.an256;

		pmemobj_tx_add_range_direct(ref, sizeof(TOID(art_node_u)));

		for (int i = 0; i < 256; i++) {
			if (D_RO(n)->keys[i]) {
				D_RW(newnode)->children[i] =
					D_RO(n)->children[D_RO(n)->keys[i] - 1];
			}
		}
		copy_header(&(D_RW(newnode)->n), n_an);
		*ref = newnode_u;
		TX_FREE(n);
		add_child256(pop, newnode, ref, c, child);
	}
}

static void
add_child16(PMEMobjpool *pop, TOID(art_node16) n, TOID(art_node_u)*ref,
	unsigned char c, TOID(art_node_u) child)
{
	art_node *n_an;

	n_an = &(D_RW(n)->n);
	if (n_an->num_children < 16) {
		__m128i cmp;

		TX_ADD(n);

		// Compare the key to all 16 stored keys
		cmp = _mm_cmplt_epi8(_mm_set1_epi8(c),
			    _mm_loadu_si128((__m128i *)(D_RO(n)->keys)));

		// Use a mask to ignore children that don't exist
		unsigned mask = (1 << n_an->num_children) - 1;
		unsigned bitfield = _mm_movemask_epi8(cmp) & mask;

		// Check if less than any
		unsigned idx;
		if (bitfield) {
			idx = __builtin_ctz(bitfield);
			memmove(&(D_RW(n)->keys[idx + 1]),
			    &(D_RO(n)->keys[idx]),
			    n_an->num_children - idx);
			PMEMOIDmove(&(D_RW(n)->children[idx + 1].oid),
			    &(D_RW(n)->children[idx].oid),
			    n_an->num_children - idx);
		} else {
			idx = n_an->num_children;
		}

		// Set the child
		D_RW(n)->keys[idx] = c;
		D_RW(n)->children[idx] = child;
		n_an->num_children++;

	} else {
		TOID(art_node_u) newnode_u = alloc_node(pop, NODE48);
		TOID(art_node48) newnode = D_RO(newnode_u)->u.an48;

		// Copy the child pointers and populate the key map
		PMEMOIDcopy(&(D_RW(newnode)->children[0].oid),
		    &(D_RO(n)->children[0].oid),
		    n_an->num_children);
		for (int i = 0; i < n_an->num_children; i++) {
			D_RW(newnode)->keys[D_RO(n)->keys[i]] = i + 1;
		}
		copy_header(&(D_RW(newnode))->n, n_an);
		*ref = newnode_u;
		TX_FREE(n);
		add_child48(pop, newnode, ref, c, child);
	}
}

static void
add_child4(PMEMobjpool *pop, TOID(art_node4) n, TOID(art_node_u) *ref,
	unsigned char c, TOID(art_node_u) child)
{
	art_node *n_an;

	n_an = &(D_RW(n)->n);
	if (n_an->num_children < 4) {
		int idx;
		TX_ADD(n);
		for (idx = 0; idx < n_an->num_children; idx++) {
			if (c < D_RO(n)->keys[idx]) break;
		}

		// Shift to make room
		memmove(D_RW(n)->keys + idx + 1, D_RO(n)->keys + idx,
		    n_an->num_children - idx);
		assert((idx + 1) < 4);
		PMEMOIDmove(&(D_RW(n)->children[idx + 1].oid),
		    &(D_RW(n)->children[idx].oid),
		    n_an->num_children - idx);

		// Insert element
		D_RW(n)->keys[idx] = c;
		D_RW(n)->children[idx] = child;
		n_an->num_children++;
	} else {
		TOID(art_node_u) newnode_u = alloc_node(pop, NODE16);
		TOID(art_node16) newnode = D_RO(newnode_u)->u.an16;

		pmemobj_tx_add_range_direct(ref, sizeof(TOID(art_node_u)));

		// Copy the child pointers and the key map
		PMEMOIDcopy(&(D_RW(newnode)->children[0].oid),
		    &(D_RO(n)->children[0].oid), n_an->num_children);
		memcpy(D_RW(newnode)->keys, D_RO(n)->keys, n_an->num_children);
		copy_header(&(D_RW(newnode)->n), n_an);
		*ref = newnode_u;
		TX_FREE(n);
		add_child16(pop, newnode, ref, c, child);
	}
}

static void
add_child(PMEMobjpool *pop, TOID(art_node_u) n, TOID(art_node_u) *ref,
	unsigned char c, TOID(art_node_u) child)
{
	switch (D_RO(n)->art_node_type) {
	case NODE4:
		add_child4(pop, D_RO(n)->u.an4, ref, c, child);
		break;
	case NODE16:
		add_child16(pop, D_RO(n)->u.an16, ref, c, child);
		break;
	case NODE48:
		add_child48(pop, D_RO(n)->u.an48, ref, c, child);
		break;
	case NODE256:
		add_child256(pop, D_RO(n)->u.an256, ref, c, child);
		break;
	default:
		abort();
	}
}

static int
prefix_mismatch(TOID(art_node_u) n, unsigned char *key, int key_len, int depth)
{
	const art_node *n_an;
	int max_cmp;
	int idx;

	switch (D_RO(n)->art_node_type) {
	case NODE4:    n_an = &(D_RO(D_RO(n)->u.an4)->n);   break;
	case NODE16:   n_an = &(D_RO(D_RO(n)->u.an16)->n);  break;
	case NODE48:   n_an = &(D_RO(D_RO(n)->u.an48)->n);  break;
	case NODE256:  n_an = &(D_RO(D_RO(n)->u.an256)->n); break;
	default: return 0;
	}
	max_cmp = min(min(MAX_PREFIX_LEN, n_an->partial_len), key_len - depth);
	for (idx = 0; idx < max_cmp; idx++) {
		if (n_an->partial[idx] != key[depth + idx])
			return idx;
	}

	// If the prefix is short we can avoid finding a leaf
	if (n_an->partial_len > MAX_PREFIX_LEN) {
		// Prefix is longer than what we've checked, find a leaf
		TOID(art_leaf) l = minimum(n);
		max_cmp = min(D_RO(D_RO(l)->key)->len, key_len) - depth;
		for (; idx < max_cmp; idx++) {
			if (D_RO(D_RO(l)->key)->s[idx + depth] !=
			    key[depth + idx])
				return idx;
		}
	}
	return idx;
}

static TOID(var_string)
recursive_insert(PMEMobjpool *pop, TOID(art_node_u) n, TOID(art_node_u) *ref,
	const unsigned char *key, int key_len,
	void *value, int val_len, int depth, int *old)
{
	art_node *n_an;
	TOID(var_string) retval;

	// If we are at a NULL node, inject a leaf
	if (TOID_IS_NULL(n)) {
		*ref = make_leaf(pop, key, key_len, value, val_len);
		TX_ADD(*ref);
		SET_LEAF(D_RW(*ref));
		retval = null_var_string;
		return retval;
	}

	// If we are at a leaf, we need to replace it with a node
	if (IS_LEAF(D_RO(n))) {
		TOID(art_leaf)l = D_RO(n)->u.al;

		// Check if we are updating an existing value
		if (!leaf_matches(l, key, key_len, depth)) {
			*old = 1;
			retval = D_RO(l)->value;
			TX_ADD(D_RW(l)->value);
			COPY_BLOB(D_RW(l)->value, value, val_len);
			return retval;
		}

		// New value, we must split the leaf into a node4
		pmemobj_tx_add_range_direct(ref,
		    sizeof(TOID(art_node_u)));
		TOID(art_node_u) newnode_u = alloc_node(pop, NODE4);
		TOID(art_node4)  newnode = D_RO(newnode_u)->u.an4;
		art_node *newnode_n = &(D_RW(newnode)->n);

		// Create a new leaf

		TOID(art_node_u) l2_u =
		    make_leaf(pop, key, key_len, value, val_len);
		TOID(art_leaf) l2 = D_RO(l2_u)->u.al;

		// Determine longest prefix
		int longest_prefix =
		    longest_common_prefix(l, l2, depth);
		newnode_n->partial_len = longest_prefix;
		memcpy(newnode_n->partial, key + depth,
		    min(MAX_PREFIX_LEN, longest_prefix));
		// Add the leafs to the newnode node4
		*ref = newnode_u;
		add_child4(pop, newnode, ref,
		    D_RO(D_RO(l)->key)->s[depth + longest_prefix],
		    n);
		add_child4(pop, newnode, ref,
		    D_RO(D_RO(l2)->key)->s[depth + longest_prefix],
		    l2_u);
		return null_var_string;
	}

	// Check if given node has a prefix
	switch (D_RO(n)->art_node_type) {
	case NODE4:   n_an = &(D_RW(D_RW(n)->u.an4)->n); break;
	case NODE16:  n_an = &(D_RW(D_RW(n)->u.an16)->n); break;
	case NODE48:  n_an = &(D_RW(D_RW(n)->u.an48)->n); break;
	case NODE256: n_an = &(D_RW(D_RW(n)->u.an256)->n); break;
	default: abort();
	}
	if (n_an->partial_len) {
		// Determine if the prefixes differ, since we need to split
		int prefix_diff =
		    prefix_mismatch(n, (unsigned char *)key, key_len, depth);
		if ((uint32_t)prefix_diff >= n_an->partial_len) {
			depth += n_an->partial_len;
			goto RECURSE_SEARCH;
		}

		// Create a new node
		pmemobj_tx_add_range_direct(ref,
		    sizeof(TOID(art_node_u)));
		pmemobj_tx_add_range_direct(n_an, sizeof(art_node));
		TOID(art_node_u) newnode_u = alloc_node(pop, NODE4);
		TOID(art_node4)  newnode = D_RO(newnode_u)->u.an4;
		art_node *newnode_n = &(D_RW(newnode)->n);

		*ref = newnode_u;
		newnode_n->partial_len = prefix_diff;
		memcpy(newnode_n->partial, n_an->partial,
		    min(MAX_PREFIX_LEN, prefix_diff));

		// Adjust the prefix of the old node
		if (n_an->partial_len <= MAX_PREFIX_LEN) {
			add_child4(pop, newnode, ref,
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
			TOID(art_leaf) l = minimum(n);
			add_child4(pop, newnode, ref,
			    D_RO(D_RO(l)->key)->s[depth + prefix_diff],
			    n);
			dst = n_an->partial;
			src =
		    &(D_RO(D_RO(l)->key)->s[depth + prefix_diff + 1 ]);
			len = min(MAX_PREFIX_LEN, n_an->partial_len);

			memcpy(dst, src, len);
		}

		// Insert the new leaf
		TOID(art_node_u) l =
		    make_leaf(pop, key, key_len, value, val_len);
		SET_LEAF(D_RW(l));
		add_child4(pop, newnode, ref, key[depth + prefix_diff], l);
		return null_var_string;
	}

RECURSE_SEARCH:;

	// Find a child to recurse to
	TOID(art_node_u) *child = find_child(n, key[depth]);
	if (!TOID_IS_NULL(*child)) {
		return recursive_insert(pop, *child, child,
			    key, key_len, value, val_len, depth + 1, old);
	}

	// No child, node goes within us
	TOID(art_node_u) l =
	    make_leaf(pop, key, key_len, value, val_len);
	SET_LEAF(D_RW(l));
	add_child(pop, n, ref, key[depth], l);
	retval = null_var_string;

	return retval;
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
TOID(var_string)
art_insert(PMEMobjpool *pop,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	int old_val = 0;
	TOID(var_string) old;
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
	TOID(art_node256) n, TOID(art_node_u) *ref, unsigned char c)
{
	art_node *n_an = &(D_RW(n)->n);

	TX_ADD(n);

	D_RW(n)->children[c].oid = OID_NULL;
	n_an->num_children--;

	// Resize to a node48 on underflow, not immediately to prevent
	// trashing if we sit on the 48/49 boundary
	if (n_an->num_children == 37) {
		TOID(art_node_u) newnode_u = alloc_node(pop, NODE48);
		TOID(art_node48) newnode_an48 = D_RO(newnode_u)->u.an48;

		pmemobj_tx_add_range_direct(ref, sizeof(TOID(art_node_u)));

		*ref = newnode_u;
		copy_header(&(D_RW(newnode_an48)->n), n_an);

		int pos = 0;
		for (int i = 0; i < 256; i++) {
			if (!TOID_IS_NULL(D_RO(n)->children[i])) {
				assert(pos < 48);
				D_RW(newnode_an48)->children[pos] =
				    D_RO(n)->children[i];
				D_RW(newnode_an48)->keys[i] = pos + 1;
				pos++;
			}
		}
		TX_FREE(n);
	}
}

static void
remove_child48(PMEMobjpool *pop,
	TOID(art_node48) n, TOID(art_node_u) *ref, unsigned char c)
{
	int pos = D_RO(n)->keys[c];
	art_node *n_an = &(D_RW(n)->n);

	TX_ADD(n);

	D_RW(n)->keys[c] = 0;
	D_RW(n)->children[pos - 1].oid = OID_NULL;
	n_an->num_children--;

	if (n_an->num_children == 12) {
		TOID(art_node_u) newnode_u = alloc_node(pop, NODE16);
		TOID(art_node16) newnode_an16 = D_RO(newnode_u)->u.an16;

		pmemobj_tx_add_range_direct(ref, sizeof(TOID(art_node_u)));

		*ref = newnode_u;
		copy_header(&(D_RW(newnode_an16)->n), n_an);

		int child = 0;
		for (int i = 0; i < 256; i++) {
			pos = D_RO(n)->keys[i];
			if (pos) {
				assert(child < 16);
				D_RW(newnode_an16)->keys[child] = i;
				D_RW(newnode_an16)->children[child] =
				    D_RO(n)->children[pos - 1];
				child++;
			}
		}
		TX_FREE(n);
	}
}

static void
remove_child16(PMEMobjpool *pop,
	TOID(art_node16) n, TOID(art_node_u) *ref, TOID(art_node_u) *l)
{
	int pos = l - &(D_RO(n)->children[0]);
	uint8_t num_children = ((D_RW(n)->n).num_children);

	TX_ADD(n);

	memmove(D_RW(n)->keys + pos, D_RO(n)->keys + pos + 1,
	    num_children - 1 - pos);
	memmove(D_RW(n)->children + pos,
	    D_RO(n)->children + pos + 1,
	    (num_children - 1 - pos) * sizeof(void *));
	((D_RW(n)->n).num_children)--;

	if (--num_children == 3) {
		TOID(art_node_u) newnode_u	 = alloc_node(pop, NODE4);
		TOID(art_node4)  newnode_an4 = D_RO(newnode_u)->u.an4;

		pmemobj_tx_add_range_direct(ref, sizeof(TOID(art_node_u)));

		*ref = newnode_u;
		copy_header(&(D_RW(newnode_an4)->n), &(D_RW(n)->n));
		memcpy(D_RW(newnode_an4)->keys, D_RO(n)->keys, 4);
		memcpy(D_RW(newnode_an4)->children,
		    D_RO(n)->children, 4 * sizeof(TOID(art_node_u)));
		TX_FREE(n);
	}
}

static void
remove_child4(PMEMobjpool *pop,
	TOID(art_node4) n, TOID(art_node_u) *ref, TOID(art_node_u) *l)
{
	int pos = l - &(D_RO(n)->children[0]);
	uint8_t *num_children = &((D_RW(n)->n).num_children);

	TX_ADD(n);

	memmove(D_RW(n)->keys + pos, D_RO(n)->keys + pos + 1,
	    *num_children - 1 - pos);
	memmove(D_RW(n)->children + pos, D_RO(n)->children + pos + 1,
	    (*num_children - 1 - pos) * sizeof(void *));
	(*num_children)--;

	// Remove nodes with only a single child
	if (*num_children == 1) {
		TOID(art_node_u) child_u = D_RO(n)->children[0];
		art_node *child = &(D_RW(D_RW(child_u)->u.an4)->n);

		pmemobj_tx_add_range_direct(ref, sizeof(TOID(art_node_u)));

		if (!IS_LEAF(D_RO(child_u))) {
			// Concatenate the prefixes
			int prefix = (D_RW(n)->n).partial_len;
			if (prefix < MAX_PREFIX_LEN) {
				(D_RW(n)->n).partial[prefix] =
				    D_RO(n)->keys[0];
				prefix++;
			}
			if (prefix < MAX_PREFIX_LEN) {
				int sub_prefix = min(child->partial_len,
				    MAX_PREFIX_LEN - prefix);
				memcpy((D_RW(n)->n).partial + prefix,
				    child->partial, sub_prefix);
				prefix += sub_prefix;
			}

			// Store the prefix in the child
			memcpy(child->partial,
			    (D_RO(n)->n).partial, min(prefix, MAX_PREFIX_LEN));
			child->partial_len += (D_RO(n)->n).partial_len + 1;
		}
		*ref = child_u;
		TX_FREE(n);
	}
}

static void
remove_child(PMEMobjpool *pop,
	TOID(art_node_u) n, TOID(art_node_u) *ref,
	unsigned char c, TOID(art_node_u) *l)
{
	switch (D_RO(n)->art_node_type) {
	case NODE4:
		return remove_child4(pop,   D_RO(n)->u.an4,   ref, l);
	case NODE16:
		return remove_child16(pop,  D_RO(n)->u.an16,  ref, l);
	case NODE48:
		return remove_child48(pop,  D_RO(n)->u.an48,  ref, c);
	case NODE256:
		return remove_child256(pop, D_RO(n)->u.an256, ref, c);
	default:
		abort();
	}
}

static TOID(art_leaf)
recursive_delete(PMEMobjpool *pop,
	TOID(art_node_u) n, TOID(art_node_u) *ref,
	const unsigned char *key, int key_len, int depth)
{
	const art_node *n_an;

	// Search terminated
	if (TOID_IS_NULL(n))
		return null_art_leaf;

	// Handle hitting a leaf node
	if (IS_LEAF(D_RO(n))) {
		TOID(art_leaf) l = D_RO(n)->u.al;
		if (!leaf_matches(l, key, key_len, depth)) {
			*ref = null_art_node_u;
			return l;
		}
		return null_art_leaf;
	}

	// get art_node component
	switch (D_RO(n)->art_node_type) {
	case NODE4:   n_an = &(D_RO(D_RO(n)->u.an4)->n); break;
	case NODE16:  n_an = &(D_RO(D_RO(n)->u.an16)->n); break;
	case NODE48:  n_an = &(D_RO(D_RO(n)->u.an48)->n); break;
	case NODE256: n_an = &(D_RO(D_RO(n)->u.an256)->n); break;
	default: abort();
	}

	// Bail if the prefix does not match
	if (n_an->partial_len) {
		int prefix_len = check_prefix(n_an, key, key_len, depth);
		if (prefix_len != min(MAX_PREFIX_LEN, n_an->partial_len)) {
			return null_art_leaf;
		}
		depth = depth + n_an->partial_len;
	}

	// Find child node
	TOID(art_node_u) *child = find_child(n, key[depth]);
	if (TOID_IS_NULL(*child))
		return null_art_leaf;

	// If the child is leaf, delete from this node
	if (IS_LEAF(D_RO(*child))) {
		TOID(art_leaf)l = D_RO(*child)->u.al;
		if (!leaf_matches(l, key, key_len, depth)) {
			remove_child(pop, n, ref, key[depth], child);
			return l;
		}
		return null_art_leaf;
	} else {
		// Recurse
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
TOID(var_string)
art_delete(PMEMobjpool *pop,
	const unsigned char *key, int key_len)
{
	TOID(struct art_tree_root)root = POBJ_ROOT(pop, struct art_tree_root);
	TOID(art_leaf) l;
	TOID(var_string) retval;

	retval = null_var_string;

	TX_BEGIN(pop) {
		TX_ADD(root);
		l = recursive_delete(pop, D_RO(root)->root,
		    &D_RW(root)->root, key, key_len, 0);
		if (!TOID_IS_NULL(l)) {
			D_RW(root)->size--;
			TOID(var_string)old = D_RO(l)->value;
			TX_FREE(l);
			retval = old;
		}
	} TX_ONABORT {
		abort();
	} TX_END

	return retval;
}

// Recursively iterates over the tree
static int
recursive_iter(TOID(art_node_u)n, art_callback cb, void *data)
{
	const art_node    *n_an;
	TOID(art_node4)   an4;
	TOID(art_node16)  an16;
	TOID(art_node48)  an48;
	TOID(art_node256) an256;
	TOID(art_leaf)    l;
	TOID(var_string)  key;
	TOID(var_string)  value;
	cb_data cbd;

	// Handle base cases
	if (TOID_IS_NULL(n)) {
		return 0;
	}

	cbd.node = n;
	cbd.child_idx = -1;
	if (IS_LEAF(D_RO(n))) {
		l = D_RO(n)->u.al;
		key = D_RO(l)->key;
		value = D_RO(l)->value;
		return cb(&cbd, D_RO(key)->s, D_RO(key)->len,
			    D_RO(value)->s, D_RO(value)->len);
	}

	int idx, res;
	switch (D_RO(n)->art_node_type) {
	case NODE4:
		an4 = D_RO(n)->u.an4;
		n_an = &(D_RO(an4)->n);
		for (int i = 0; i < n_an->num_children; i++) {
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter(D_RO(an4)->children[i], cb, data);
			if (res)
				return res;
		}
		break;

	case NODE16:
		an16 = D_RO(n)->u.an16;
		n_an = &(D_RO(an16)->n);
		for (int i = 0; i < n_an->num_children; i++) {
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter(D_RO(an16)->children[i], cb, data);
			if (res)
				return res;
		}
		break;

	case NODE48:
		an48 = D_RO(n)->u.an48;
		for (int i = 0; i < 256; i++) {
			idx = D_RO(an48)->keys[i];
			if (!idx)
				continue;

			cbd.child_idx = idx - 1;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter(D_RO(an48)->children[idx - 1],
				    cb, data);
			if (res)
				return res;
		}
		break;

	case NODE256:
		an256 = D_RO(n)->u.an256;
		for (int i = 0; i < 256; i++) {
			if (TOID_IS_NULL(D_RO(an256)->children[i]))
				continue;
			cbd.child_idx = i;
			cb(&cbd, NULL, 0, NULL, 0);
			res = recursive_iter(D_RO(an256)->children[i],
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

#ifdef LIBART_ITER_PREFIX /* {  */
/*
 * Checks if a leaf prefix matches
 * @return 0 on success.
 */
static int
leaf_prefix_matches(TOID(art_leaf) n,
	const unsigned char *prefix, int prefix_len)
{
	// Fail if the key length is too short
	if (D_RO(D_RO(n)->key)->len < (uint32_t)prefix_len)
		return 1;

	// Compare the keys
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
		// Might be a leaf
		if (IS_LEAF(n)) {
			n = LEAF_RAW(n);
			// Check if the expanded path matches
			if (!leaf_prefix_matches((art_leaf *)n, key, key_len)) {
				art_leaf *l = (art_leaf *)n;
				return cb(data,
					    (const unsigned char *)l->key,
					    l->key_len, l->value);
			}
			return 0;
		}

		// If the depth matches the prefix, we need to handle this node
		if (depth == key_len) {
			art_leaf *l = minimum(n);
			if (!leaf_prefix_matches(l, key, key_len))
				return recursive_iter(n, cb, data);
			return 0;
		}

		// Bail if the prefix does not match
		if (n->partial_len) {
			prefix_len = prefix_mismatch(n, key, key_len, depth);

			// If there is no match, search is terminated
			if (!prefix_len)
				return 0;

			// If we've matched the prefix, iterate on this node
			else if (depth + prefix_len == key_len) {
				return recursive_iter(n, cb, data);
			}

			// if there is a full match, go deeper
			depth = depth + n->partial_len;
		}

		// Recursively search
		child = find_child(n, key[depth]);
		n = (child) ? *child : NULL;
		depth++;
	}
	return 0;
}
#endif /* } LIBART_ITER_PREFIX */

int
fill_leaf(PMEMobjpool *pop, TOID(art_leaf) al,
	const unsigned char *key, int key_len, void *value, int val_len)
{
	int retval = 0;
	size_t l_key;
	size_t l_val;

	TOID(var_string) Tkey;
	TOID(var_string) Tval;

	l_key = (sizeof(var_string) + key_len);
	l_val = (sizeof(var_string) + val_len);
	Tkey = TX_ALLOC(var_string, l_key);
	Tval = TX_ALLOC(var_string, l_val);

	COPY_BLOB(Tkey, key,   key_len);
	COPY_BLOB(Tval, value, val_len);

	D_RW(al)->key = Tkey;
	D_RW(al)->value = Tval;

	return retval;
}
