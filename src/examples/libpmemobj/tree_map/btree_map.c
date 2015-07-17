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
 * btree_map.c -- textbook implementation of btree /w preemptive splitting
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "tree_map.h"

TOID_DECLARE(struct tree_map_node, TREE_MAP_TYPE_OFFSET + 1);

#define	BTREE_ORDER 8 /* can't be odd */
#define	BTREE_MIN ((BTREE_ORDER / 2) - 1) /* min number of keys per node */

struct tree_map_node_item {
	uint64_t key;
	PMEMoid value;
};

#define	EMPTY_ITEM ((struct tree_map_node_item)\
		{0, OID_NULL})

struct tree_map_node {
	int n; /* number of occupied slots */
	struct tree_map_node_item items[BTREE_ORDER - 1];
	TOID(struct tree_map_node) slots[BTREE_ORDER];
};

struct tree_map {
	TOID(struct tree_map_node) root;
};

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
 * tree_map_insert_item_at -- (internal) inserts an item at position
 */
static void
tree_map_insert_item_at(TOID(struct tree_map_node) node, int pos,
	struct tree_map_node_item item)
{
	D_RW(node)->items[pos] = item;
	D_RW(node)->n += 1;
}

/*
 * tree_map_insert_empty -- (internal) inserts an item into an empty node
 */
static void
tree_map_insert_empty(TOID(struct tree_map) map, struct tree_map_node_item item)
{
	TX_ADD_FIELD(map, root);
	D_RW(map)->root = TX_ZNEW(struct tree_map_node);

	tree_map_insert_item_at(D_RO(map)->root, 0, item);
}

/*
 * tree_map_insert_node -- (internal) inserts and makes space for new node
 */
static void
tree_map_insert_node(TOID(struct tree_map_node) node, int p,
	struct tree_map_node_item item,
	TOID(struct tree_map_node) left, TOID(struct tree_map_node) right)
{
	TX_ADD(node);
	if (D_RO(node)->items[p].key != 0) { /* move all existing data */
		memmove(&D_RW(node)->items[p + 1], &D_RW(node)->items[p],
		sizeof (struct tree_map_node_item) * ((BTREE_ORDER - 2 - p)));

		memmove(&D_RW(node)->slots[p + 1], &D_RW(node)->slots[p],
		sizeof (TOID(struct tree_map_node)) * ((BTREE_ORDER - 1 - p)));
	}
	D_RW(node)->slots[p] = left;
	D_RW(node)->slots[p + 1] = right;
	tree_map_insert_item_at(node, p, item);
}

/*
 * tree_map_create_split_node -- (internal) splits a node into two
 */
static TOID(struct tree_map_node)
tree_map_create_split_node(TOID(struct tree_map_node) node,
	struct tree_map_node_item *m)
{
	TOID(struct tree_map_node) right = TX_ZNEW(struct tree_map_node);

	int c = (BTREE_ORDER / 2);
	*m = D_RO(node)->items[c - 1]; /* select median item */
	D_RW(node)->items[c - 1] = EMPTY_ITEM;

	/* move everything right side of median to the new node */
	for (int i = c; i < BTREE_ORDER; ++i) {
		if (i != BTREE_ORDER - 1) {
			D_RW(right)->items[D_RW(right)->n++] =
				D_RO(node)->items[i];

			D_RW(node)->items[i] = EMPTY_ITEM;
		}
		D_RW(right)->slots[i - c] = D_RO(node)->slots[i];
		D_RW(node)->slots[i] = TOID_NULL(struct tree_map_node);
	}
	D_RW(node)->n = c - 1;

	return right;
}

/*
 * tree_map_find_dest_node -- (internal) finds a place to insert the new key at
 */
static TOID(struct tree_map_node)
tree_map_find_dest_node(TOID(struct tree_map) map, TOID(struct tree_map_node) n,
	TOID(struct tree_map_node) parent, uint64_t key, int *p)
{
	if (D_RO(n)->n == BTREE_ORDER - 1) { /* node is full, perform a split */
		struct tree_map_node_item m;
		TOID(struct tree_map_node) right =
			tree_map_create_split_node(n, &m);

		if (!TOID_IS_NULL(parent)) {
			tree_map_insert_node(parent, *p, m, n, right);
			if (key > m.key) /* select node to continue search */
				n = right;
		} else { /* replacing root node, the tree grows in height */
			TOID(struct tree_map_node) up =
				TX_ZNEW(struct tree_map_node);
			D_RW(up)->n = 1;
			D_RW(up)->items[0] = m;
			D_RW(up)->slots[0] = n;
			D_RW(up)->slots[1] = right;

			TX_ADD_FIELD(map, root);
			D_RW(map)->root = up;
			n = up;
		}
	}

	for (int i = 0; i < BTREE_ORDER; ++i) {
		if (i == BTREE_ORDER - 1 || D_RO(n)->items[i].key == 0 ||
			D_RO(n)->items[i].key > key) {
			*p = i;
			return TOID_IS_NULL(D_RO(n)->slots[i]) ? n :
				tree_map_find_dest_node(map,
					D_RO(n)->slots[i], n, key, p);
		}
	}

	return TOID_NULL(struct tree_map_node);
}

/*
 * tree_map_insert_item -- (internal) inserts and makes space for new item
 */
static void
tree_map_insert_item(TOID(struct tree_map_node) node, int p,
	struct tree_map_node_item item)
{
	TX_ADD(node);
	if (D_RO(node)->items[p].key != 0) {
		memmove(&D_RW(node)->items[p + 1], &D_RW(node)->items[p],
		sizeof (struct tree_map_node_item) * ((BTREE_ORDER - 2 - p)));
	}
	tree_map_insert_item_at(node, p, item);
}

/*
 * tree_map_insert -- inserts a new key-value pair into the map
 */
int
tree_map_insert(PMEMobjpool *pop,
	TOID(struct tree_map) map, uint64_t key, PMEMoid value)
{
	struct tree_map_node_item item = {key, value};
	TX_BEGIN(pop) {
		if (tree_map_is_empty(map)) {
			tree_map_insert_empty(map, item);
		} else {
			int p; /* position at the dest node to insert */
			TOID(struct tree_map_node) parent =
				TOID_NULL(struct tree_map_node);
			TOID(struct tree_map_node) dest =
				tree_map_find_dest_node(map, D_RW(map)->root,
					parent, key, &p);

			tree_map_insert_item(dest, p, item);
		}
	} TX_END

	return 0;
}

/*
 * tree_map_rotate_right -- (internal) takes one element from right sibling
 */
static void
tree_map_rotate_right(TOID(struct tree_map_node) rsb,
	TOID(struct tree_map_node) node,
	TOID(struct tree_map_node) parent, int p)
{
	/* move the separator from parent to the deficient node */
	struct tree_map_node_item sep = D_RO(parent)->items[p];
	tree_map_insert_item(node, D_RO(node)->n, sep);

	/* the first element of the right sibling is the new separator */
	TX_ADD_FIELD(parent, items[p]);
	D_RW(parent)->items[p] = D_RO(rsb)->items[0];

	/* the nodes are not necessarily leafs, so copy also the slot */
	TX_ADD_FIELD(node, slots[D_RO(node)->n]);
	D_RW(node)->slots[D_RO(node)->n] = D_RO(rsb)->slots[0];

	TX_ADD(rsb);
	D_RW(rsb)->n -= 1; /* it loses one element, but still > min */

	/* move all existing elements back by one array slot */
	memmove(D_RW(rsb)->items, D_RO(rsb)->items + 1,
		sizeof (struct tree_map_node_item) * (D_RO(rsb)->n));
	memmove(D_RW(rsb)->slots, D_RO(rsb)->slots + 1,
		sizeof (TOID(struct tree_map_node)) * (D_RO(rsb)->n + 1));
}

/*
 * tree_map_rotate_left -- (internal) takes one element from left sibling
 */
static void
tree_map_rotate_left(TOID(struct tree_map_node) lsb,
	TOID(struct tree_map_node) node,
	TOID(struct tree_map_node) parent, int p)
{
	/* move the separator from parent to the deficient node */
	struct tree_map_node_item sep = D_RO(parent)->items[p - 1];
	tree_map_insert_item(node, 0, sep);

	/* the last element of the left sibling is the new separator */
	TX_ADD_FIELD(parent, items[p - 1]);
	D_RW(parent)->items[p - 1] = D_RO(lsb)->items[D_RO(lsb)->n - 1];

	TX_ADD(node);
	/* rotate the node children */
	memmove(D_RW(node)->slots + 1, D_RO(node)->slots,
		sizeof (TOID(struct tree_map_node)) * (D_RO(node)->n));

	/* the nodes are not necessarily leafs, so copy also the slot */
	D_RW(node)->slots[0] = D_RO(lsb)->slots[D_RO(lsb)->n];

	TX_ADD_FIELD(lsb, n);
	D_RW(lsb)->n -= 1; /* it loses one element, but still > min */
}

/*
 * tree_map_merge -- (internal) merges node and right sibling
 */
static void
tree_map_merge(TOID(struct tree_map) map, TOID(struct tree_map_node) rn,
	TOID(struct tree_map_node) node,
	TOID(struct tree_map_node) parent, int p)
{
	struct tree_map_node_item sep = D_RO(parent)->items[p];

	TX_ADD(node);
	/* add separator to the deficient node */
	D_RW(node)->items[D_RW(node)->n++] = sep;

	/* copy right sibling data to node */
	memcpy(&D_RW(node)->items[D_RO(node)->n], D_RO(rn)->items,
	sizeof (struct tree_map_node_item) * D_RO(rn)->n);
	memcpy(&D_RW(node)->slots[D_RO(node)->n], D_RO(rn)->slots,
	sizeof (TOID(struct tree_map_node)) * (D_RO(rn)->n + 1));

	D_RW(node)->n += D_RO(rn)->n;

	TX_FREE(rn); /* right node is now empty */

	TX_ADD(parent);
	D_RW(parent)->n -= 1;

	/* move everything to the right of the separator by one array slot */
	memmove(D_RW(parent)->items + p, D_RW(parent)->items + p + 1,
	sizeof (struct tree_map_node_item) * (D_RO(parent)->n - p));

	memmove(D_RW(parent)->slots + p + 1, D_RW(parent)->slots + p + 2,
	sizeof (TOID(struct tree_map_node)) * (D_RO(parent)->n - p + 1));

	/* if the parent is empty then the tree shrinks in height */
	if (D_RO(parent)->n == 0 && TOID_EQUALS(parent, D_RO(map)->root)) {
		TX_ADD(map);
		TX_FREE(D_RO(map)->root);
		D_RW(map)->root = node;
	}
}

/*
 * tree_map_rebalance -- (internal) performs tree rebalance
 */
static void
tree_map_rebalance(TOID(struct tree_map) map, TOID(struct tree_map_node) node,
	TOID(struct tree_map_node) parent, int p)
{
	TOID(struct tree_map_node) rsb = p >= D_RO(parent)->n ?
		TOID_NULL(struct tree_map_node) : D_RO(parent)->slots[p + 1];
	TOID(struct tree_map_node) lsb = p == 0 ?
		TOID_NULL(struct tree_map_node) : D_RO(parent)->slots[p - 1];

	if (!TOID_IS_NULL(rsb) && D_RO(rsb)->n > BTREE_MIN)
		tree_map_rotate_right(rsb, node, parent, p);
	else if (!TOID_IS_NULL(lsb) && D_RO(lsb)->n > BTREE_MIN)
		tree_map_rotate_left(lsb, node, parent, p);
	else if (TOID_IS_NULL(rsb)) /* always merge with rightmost node */
		tree_map_merge(map, node, lsb, parent, p - 1);
	else
		tree_map_merge(map, rsb, node, parent, p);
}

/*
 * tree_map_remove_from_node -- (internal) removes element from node
 */
static void
tree_map_remove_from_node(TOID(struct tree_map) map,
	TOID(struct tree_map_node) node,
	TOID(struct tree_map_node) parent, int p)
{
	if (TOID_IS_NULL(D_RO(node)->slots[0])) { /* leaf */
		TX_ADD(node);
		if (p == BTREE_ORDER - 2)
			D_RW(node)->items[p] = EMPTY_ITEM;
		else if (D_RO(node)->n != 1) {
			memmove(&D_RW(node)->items[p],
				&D_RW(node)->items[p + 1],
				sizeof (struct tree_map_node_item) *
				(D_RO(node)->n - p));
		}
		D_RW(node)->n -= 1;
		return;
	}

	/* can't delete from non-leaf nodes, remove from right child */
	TOID(struct tree_map_node) rchild = D_RW(node)->slots[p + 1];
	TX_ADD_FIELD(node, items[p]);
	D_RW(node)->items[p] = D_RO(rchild)->items[0];
	tree_map_remove_from_node(map, rchild, node, 0);
	if (D_RO(rchild)->n < BTREE_MIN) /* right child can be deficient now */
		tree_map_rebalance(map, rchild, node, p + 1);
}

/*
 * tree_map_remove_item -- (internal) removes item from node
 */
static PMEMoid
tree_map_remove_item(TOID(struct tree_map) map, TOID(struct tree_map_node) node,
	TOID(struct tree_map_node) parent, uint64_t key, int p)
{
	PMEMoid ret = OID_NULL;
	int i = 0;
	for (; i <= D_RO(node)->n; ++i) {
		if (i == D_RO(node)->n || D_RO(node)->items[i].key > key) {
			ret = tree_map_remove_item(map, D_RO(node)->slots[i],
				node, key, i);
			break;
		} else if (D_RO(node)->items[i].key == key) {
			tree_map_remove_from_node(map, node, parent, i);
			ret = D_RO(node)->items[i].value;
			break;
		}
	}

	/* check for deficient nodes walking up */
	if (!TOID_IS_NULL(parent) && D_RO(node)->n < BTREE_MIN)
		tree_map_rebalance(map, node, parent, p);

	return ret;
}

/*
 * tree_map_remove -- removes key-value pair from the map
 */
PMEMoid
tree_map_remove(PMEMobjpool *pop, TOID(struct tree_map) map, uint64_t key)
{
	PMEMoid ret = OID_NULL;
	TX_BEGIN(pop) {
		ret = tree_map_remove_item(map, D_RW(map)->root,
				TOID_NULL(struct tree_map_node), key, 0);
	} TX_END

	return ret;
}

/*
 * tree_map_clear_node -- (internal) removes all elements from the node
 */
static void
tree_map_clear_node(TOID(struct tree_map_node) node)
{
	for (int i = 0; i < D_RO(node)->n; ++i) {
		tree_map_clear_node(D_RO(node)->slots[i]);
	}

	TX_FREE(node);
}

/*
 * tree_map_clear -- removes all elements from the map
 */
int
tree_map_clear(PMEMobjpool *pop,
	TOID(struct tree_map) map)
{
	int ret = 0;
	TX_BEGIN(pop) {
		tree_map_clear_node(D_RO(map)->root);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * tree_map_get_from_node -- (internal) searches for a value in the node
 */
static PMEMoid
tree_map_get_from_node(TOID(struct tree_map_node) node, uint64_t key)
{
	for (int i = 0; i <= D_RO(node)->n; ++i)
		if (i == D_RO(node)->n || D_RO(node)->items[i].key > key)
			return tree_map_get_from_node(
				D_RO(node)->slots[i], key);
		else if (D_RO(node)->items[i].key == key)
			return D_RO(node)->items[i].value;

	return OID_NULL;
}

/*
 * tree_map_get -- searches for a value of the key
 */
PMEMoid
tree_map_get(TOID(struct tree_map) map, uint64_t key)
{
	return tree_map_get_from_node(D_RO(map)->root, key);
}

/*
 * tree_map_foreach_node -- (internal) recursively traverses tree
 */
static int
tree_map_foreach_node(const TOID(struct tree_map_node) p,
	int (*cb)(uint64_t key, PMEMoid, void *arg), void *arg)
{
	if (TOID_IS_NULL(p))
		return 0;

	for (int i = 0; i <= D_RO(p)->n; ++i) {
		if (tree_map_foreach_node(D_RO(p)->slots[i], cb, arg) != 0)
			return 1;

		if (i != D_RO(p)->n && D_RO(p)->items[i].key != 0) {
			if (cb(D_RO(p)->items[i].key, D_RO(p)->items[i].value,
					arg) != 0)
				return 1;
		}
	}
	return 0;
}

/*
 * tree_map_foreach -- initiates recursive traversal
 */
int
tree_map_foreach(TOID(struct tree_map) map,
	int (*cb)(uint64_t key, PMEMoid value, void *arg), void *arg)
{
	return tree_map_foreach_node(D_RO(map)->root, cb, arg);
}

/*
 * tree_map_is_empty -- checks whether the tree map is empty
 */
int
tree_map_is_empty(TOID(struct tree_map) map)
{
	return TOID_IS_NULL(D_RO(map)->root) || D_RO(D_RO(map)->root)->n == 0;
}
