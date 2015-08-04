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
 * ctree_map.c -- Crit-bit trie implementation
 */

#include <assert.h>
#include <errno.h>
#include "tree_map.h"

#define	BIT_IS_SET(n, i) (!!((n) & (1L << (i))))

TOID_DECLARE(struct tree_map_node, TREE_MAP_TYPE_OFFSET + 1);
TOID_DECLARE(struct tree_map_leaf, TREE_MAP_TYPE_OFFSET + 2);

struct tree_map_node {
	int diff; /* most significant differing bit */
	PMEMoid slots[2]; /* slots for either internal or leaf nodes */
};

struct tree_map_leaf {
	uint64_t key;
	PMEMoid value;
};

struct tree_map {
	PMEMoid root;
};

/*
 * find_crit_bit -- (internal) finds the most significant differing bit
 */
static int
find_crit_bit(uint64_t lhs, uint64_t rhs)
{
	return 64 - __builtin_clzll(lhs ^ rhs) - 1;
}

/*
 * tree_map_new -- allocates a new crit-bit tree instance
 */
int
tree_map_new(PMEMobjpool *pop, TOID(struct tree_map) *map)
{
	int ret = 0;
	TX_BEGIN(pop) {
		pmemobj_tx_add_range_direct(map, sizeof (*map));
		*map = TX_ZNEW(struct tree_map);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * tree_map_delete -- cleanups and frees crit-bit tree instance
 */
int
tree_map_delete(PMEMobjpool *pop, TOID(struct tree_map) *map)
{
	int ret = 0;
	TX_BEGIN(pop) {
		tree_map_clear(pop, *map);
		pmemobj_tx_add_range_direct(map, sizeof (*map));
		TX_FREE(*map);
		*map = TOID_NULL(struct tree_map);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * tree_map_insert_leaf -- (internal) inserts a new leaf at the position
 */
static void
tree_map_insert_leaf(PMEMoid *p, TOID(struct tree_map_leaf) new_leaf, int diff)
{
	TOID(struct tree_map_node) new_node = TX_NEW(struct tree_map_node);
	D_RW(new_node)->diff = diff;

	int d = BIT_IS_SET(D_RO(new_leaf)->key, D_RO(new_node)->diff);

	/* insert the leaf at the direction based on the critical bit */
	D_RW(new_node)->slots[d] = new_leaf.oid;

	/* find the appropriate position in the tree to insert the node */
	TOID(struct tree_map_node) node;
	while (OID_INSTANCEOF(*p, struct tree_map_node)) {
		TOID_ASSIGN(node, *p);

		/* the critical bits have to be sorted */
		if (D_RO(node)->diff < D_RO(new_node)->diff)
			break;

		p = &D_RW(node)->slots
			[BIT_IS_SET(D_RO(new_leaf)->key, D_RO(node)->diff)];
	}

	/* insert the found destination in the other slot */
	D_RW(new_node)->slots[!d] = *p;

	pmemobj_tx_add_range_direct(p, sizeof (*p));
	*p = new_node.oid;
}

/*
 * tree_map_insert -- inserts a new key-value pair into the map
 */
int
tree_map_insert(PMEMobjpool *pop,
	TOID(struct tree_map) map, uint64_t key, PMEMoid value)
{
	PMEMoid *p = &D_RW(map)->root;
	int ret = 0;

	/* descend the path until a best matching key is found */
	TOID(struct tree_map_node) node;
	while (OID_INSTANCEOF(*p, struct tree_map_node)) {
		TOID_ASSIGN(node, *p);
		p = &D_RW(node)->slots[BIT_IS_SET(key, D_RW(node)->diff)];
	}

	TX_BEGIN(pop) {
		TOID(struct tree_map_leaf) new_leaf =
					TX_NEW(struct tree_map_leaf);

		D_RW(new_leaf)->key = key;
		D_RW(new_leaf)->value = value;

		TOID(struct tree_map_leaf) leaf;
		TOID_ASSIGN(leaf, *p);

		if (TOID_IS_NULL(leaf)) { /* root */
			pmemobj_tx_add_range_direct(p, sizeof (*p));
			*p = new_leaf.oid;
		} else if (D_RO(leaf)->key == D_RO(new_leaf)->key) {
			TX_ADD_FIELD(leaf, value); /* key already exists */
			D_RW(leaf)->value = value;
		} else {
			tree_map_insert_leaf(&D_RW(map)->root, new_leaf,
					find_crit_bit(D_RO(leaf)->key, key));
		}
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * tree_map_get_leaf -- (internal) searches for a leaf of the key
 */
static TOID(struct tree_map_leaf) *
tree_map_get_leaf(TOID(struct tree_map) map,
	uint64_t key, TOID(struct tree_map_node) **parent)
{
	PMEMoid *p = &D_RW(map)->root;
	TOID(struct tree_map_node) *node = NULL;

	while (OID_INSTANCEOF(*p, struct tree_map_node)) {
		node = (TOID(struct tree_map_node) *)p;

		p = &D_RW(*node)->slots[BIT_IS_SET(key, D_RW(*node)->diff)];
	}

	if (!OID_IS_NULL(*p) && OID_INSTANCEOF(*p, struct tree_map_leaf)) {
		TOID(struct tree_map_leaf) *leaf =
			(TOID(struct tree_map_leaf) *)p;

		if (D_RW(*leaf)->key == key) {
			if (parent)
				*parent = node;

			return leaf;
		}
	}

	return NULL;
}

/*
 * tree_map_remove -- removes key-value pair from the map
 */
PMEMoid
tree_map_remove(PMEMobjpool *pop, TOID(struct tree_map) map, uint64_t key)
{
	TOID(struct tree_map_node) *parent = NULL;
	TOID(struct tree_map_leaf) *leaf = tree_map_get_leaf(map, key, &parent);
	if (leaf == NULL || TOID_IS_NULL(*leaf))
		return OID_NULL;

	PMEMoid ret = D_RO(*leaf)->value;

	if (parent == NULL) { /* root */
		TX_BEGIN(pop) {
			TX_FREE(*leaf);
			pmemobj_tx_add_range_direct(leaf, sizeof (*leaf));
			*leaf = TOID_NULL(struct tree_map_leaf);
		} TX_END
	} else {
		/*
		 * In this situation:
		 *	 parent
		 *	/     \
		 *   LEFT   RIGHT
		 * there's no point in leaving the parent internal node
		 * so it's swapped with the remaining node and then also freed.
		 */
		TX_BEGIN(pop) {
			PMEMoid *dest = (PMEMoid *)parent;
			TOID(struct tree_map_node) node = *parent;
			pmemobj_tx_add_range_direct(dest, sizeof (*dest));
			*dest = D_RW(*parent)->slots[
				OID_EQUALS(D_RO(*parent)->slots[0], leaf->oid)];

			TX_FREE(*leaf);
			TX_FREE(node);
		} TX_END
	}

	return ret;
}

/*
 * tree_map_clear_node -- (internal) clears this node and its children
 */
static void
tree_map_clear_node(PMEMoid p)
{
	if (OID_INSTANCEOF(p, struct tree_map_node)) {
		TOID(struct tree_map_node) node;
		TOID_ASSIGN(node, p);

		tree_map_clear_node(D_RW(node)->slots[0]);
		tree_map_clear_node(D_RW(node)->slots[1]);
	}

	pmemobj_tx_free(p);
}

/*
 * tree_map_clear -- removes all elements from the map
 */
int
tree_map_clear(PMEMobjpool *pop,
	TOID(struct tree_map) map)
{
	TX_BEGIN(pop) {
		tree_map_clear_node(D_RW(map)->root);
		TX_ADD_FIELD(map, root);
		D_RW(map)->root = OID_NULL;
	} TX_END

	return 0;
}

/*
 * tree_map_get -- searches for a value of the key
 */
PMEMoid
tree_map_get(TOID(struct tree_map) map, uint64_t key)
{
	TOID(struct tree_map_leaf) *leaf = tree_map_get_leaf(map, key, NULL);

	return leaf == NULL ? OID_NULL : D_RW(*leaf)->value;
}

/*
 * tree_map_foreach_node -- (internal) recursively traverses tree
 */
static int
tree_map_foreach_node(PMEMoid p,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	int ret = 0;
	if (OID_INSTANCEOF(p, struct tree_map_leaf)) {
		TOID(struct tree_map_leaf) leaf;
		TOID_ASSIGN(leaf, p);
		ret = cb(D_RO(leaf)->key, D_RO(leaf)->value, arg);
	} else { /* struct tree_map_node */
		TOID(struct tree_map_node) node;
		TOID_ASSIGN(node, p);

		if (tree_map_foreach_node(D_RO(node)->slots[0], cb, arg) == 0)
			tree_map_foreach_node(D_RO(node)->slots[1], cb, arg);
	}

	return ret;
}

/*
 * tree_map_foreach -- initiates recursive traversal
 */
int
tree_map_foreach(TOID(struct tree_map) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	if (OID_IS_NULL(D_RO(map)->root))
		return 0;

	return tree_map_foreach_node(D_RO(map)->root, cb, arg);
}

/*
 * tree_map_is_empty -- checks whether the tree map is empty
 */
int
tree_map_is_empty(TOID(struct tree_map) map)
{
	return OID_IS_NULL(D_RO(map)->root);
}
