/*
 * Copyright 2016, Intel Corporation
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
 * rtree_map.c -- implementation of rtree
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/param.h>

#include "rtree_map.h"


TOID_DECLARE(struct tree_map_node, RTREE_MAP_TYPE_OFFSET + 1);

/* Good values: 0x10 an 0x100, but implementation is binded to 0x100 */
#ifndef ALPHABET_SIZE
#define ALPHABET_SIZE 0x100
#endif

struct tree_map_node {
	TOID(struct tree_map_node) slots[ALPHABET_SIZE];
	unsigned has_value;
	PMEMoid value;
	uint64_t key_size;
	unsigned char key[];
};

struct rtree_map {
	TOID(struct tree_map_node) root;
};

/*
 * rtree_map_new -- allocates a new rtree instance
 */
int
rtree_map_new(PMEMobjpool *pop, TOID(struct rtree_map) *map, void *arg)
{
	int ret = 0;

	TX_BEGIN(pop) {
		TX_ADD_DIRECT(map);
		*map = TX_ZNEW(struct rtree_map);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * rtree_map_clear_node -- (internal) removes all elements from the node
 */
static void
rtree_map_clear_node(TOID(struct tree_map_node) node)
{
	for (unsigned i = 0; i < ALPHABET_SIZE; i++) {
		rtree_map_clear_node(D_RO(node)->slots[i]);
	}

	pmemobj_tx_add_range(node.oid, 0,
			sizeof(struct tree_map_node) + D_RO(node)->key_size);
	TX_FREE(node);
}


/*
 * rtree_map_clear -- removes all elements from the map
 */
int
rtree_map_clear(PMEMobjpool *pop, TOID(struct rtree_map) map)
{
	int ret = 0;
	TX_BEGIN(pop) {
		rtree_map_clear_node(D_RO(map)->root);

		TX_ADD_FIELD(map, root);

		D_RW(map)->root = TOID_NULL(struct tree_map_node);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}


/*
 * rtree_map_delete -- cleanups and frees rtree instance
 */
int
rtree_map_delete(PMEMobjpool *pop, TOID(struct rtree_map) *map)
{
	int ret = 0;
	TX_BEGIN(pop) {
		rtree_map_clear(pop, *map);
		TX_ADD_DIRECT(map);
		TX_FREE(*map);
		*map = TOID_NULL(struct rtree_map);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * rtree_new_node -- (internal) inserts a node into an empty map
 */
static TOID(struct tree_map_node)
rtree_new_node(const unsigned char *key, uint64_t key_size,
		PMEMoid value, unsigned has_value)
{
	TOID(struct tree_map_node) node;

	node = TX_ZALLOC(struct tree_map_node,
			sizeof(struct tree_map_node) + key_size);

	/*
	 * !!! Here should be: D_RO(node)->value
	 * ... because we don't change map
	 */
	D_RW(node)->value = value;
	D_RW(node)->has_value = has_value;
	D_RW(node)->key_size = key_size;

	memcpy(D_RW(node)->key, key, key_size);

	return node;
}
/*
 * rtree_map_insert_empty -- (internal) inserts a node into an empty map
 */
static void
rtree_map_insert_empty(TOID(struct rtree_map) map,
	const unsigned char *key, uint64_t key_size, PMEMoid value)
{
	TX_ADD_FIELD(map, root);
	D_RW(map)->root = rtree_new_node(key, key_size, value, 1);
}

/*
 * key_comm_len -- (internal) calculate the len of common part of keys
 */
static unsigned
key_comm_len(TOID(struct tree_map_node) node,
	const unsigned char *key, uint64_t key_size)
{
	unsigned i;

	for (i = 0;
		i < MIN(key_size, D_RO(node)->key_size) &&
			key[i] == D_RO(node)->key[i];
		i++)
		;

	return i;
}

/*
 * rtree_map_insert_value -- (internal) inserts a pair into a tree
 */
static void
rtree_map_insert_value(TOID(struct tree_map_node) *node,
	const unsigned char *key, uint64_t key_size, PMEMoid value)
{
	unsigned i;

	if (TOID_IS_NULL(*node)) {
		TX_ADD_DIRECT(node);
		*node = rtree_new_node(key, key_size, value, 1);
		return;
	}

	i = key_comm_len(*node, key, key_size);

	if (i != D_RO(*node)->key_size) {
		/* Node does not exist. Let's add. */
		TOID(struct tree_map_node) orig_node = *node;
		TX_ADD_DIRECT(node);

		if (i != key_size) {
			*node = rtree_new_node(D_RO(orig_node)->key, i,
					OID_NULL, 0);
		} else {
			*node = rtree_new_node(D_RO(orig_node)->key, i,
					value, 1);
		}

		D_RW(*node)->slots[D_RO(orig_node)->key[i]] = orig_node;
		D_RW(orig_node)->key_size -= i;
		memmove(D_RW(orig_node)->key, D_RO(orig_node)->key + i,
				D_RO(orig_node)->key_size);

		if (i != key_size) {
			D_RW(*node)->slots[key[i]] =
				rtree_new_node(key + i, key_size - i, value, 1);
		}
		return;
	}

	if (i == key_size) {
		if (OID_IS_NULL(D_RO(*node)->value) || D_RO(*node)->has_value) {
			/* Just replace old value with new */
			TX_ADD_FIELD(*node, value);
			TX_ADD_FIELD(*node, has_value);

			D_RW(*node)->value = value;
			D_RW(*node)->has_value = 1;
		} else {
			/*
			 *  Ignore. By the fact current value should be
			 *  removed in advance, or handled in a different way.
			 */
		}
	} else {
		/* Recurse deeply */
		return rtree_map_insert_value(&D_RW(*node)->slots[key[i]],
				key + i, key_size - i, value);
	}
}

/*
 * rtree_map_is_empty -- checks whether the tree map is empty
 */
int
rtree_map_is_empty(PMEMobjpool *pop, TOID(struct rtree_map) map)
{
	return TOID_IS_NULL(D_RO(map)->root);
}

/*
 * rtree_map_insert -- inserts a new key-value pair into the map
 */
int
rtree_map_insert(PMEMobjpool *pop, TOID(struct rtree_map) map,
	const unsigned char *key, uint64_t key_size, PMEMoid value)
{
	int ret = 0;

	TX_BEGIN(pop) {
		if (rtree_map_is_empty(pop, map)) {
			rtree_map_insert_empty(map, key, key_size, value);
		} else {
			rtree_map_insert_value(&D_RW(map)->root,
					key, key_size, value);
		}
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * rtree_map_insert_new -- allocates a new object and inserts it into the tree
 */
int
rtree_map_insert_new(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size,
		size_t size, unsigned type_num,
		void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
		void *arg)
{
	int ret = 0;

	TX_BEGIN(pop) {
		PMEMoid n = pmemobj_tx_alloc(size, type_num);
		constructor(pop, pmemobj_direct(n), arg);
		rtree_map_insert(pop, map, key, key_size, n);
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * rtree_map_remove_node -- (internal) removes node from tree
 */
static PMEMoid
rtree_map_remove_node(TOID(struct rtree_map) map,
	TOID(struct tree_map_node) *node,
	const unsigned char *key, uint64_t key_size)
{
	unsigned i;

	if (TOID_IS_NULL(*node))
		return OID_NULL;

	i = key_comm_len(*node, key, key_size);

	if (i != D_RO(*node)->key_size)
		/* Node does not exist */
		return OID_NULL;

	if (i == key_size) {
		unsigned j;

		if (0 == D_RO(*node)->has_value)
			return OID_NULL;

		/* Node is found */
		PMEMoid ret = D_RO(*node)->value;

		/* delete node from tree */
		TX_ADD_FIELD((*node), value);
		TX_ADD_FIELD((*node), has_value);
		D_RW(*node)->value = OID_NULL;
		D_RW(*node)->has_value = 0;

		for (j = 0;
			j < ALPHABET_SIZE &&
				TOID_IS_NULL(D_RO(*node)->slots[j]);
			j++)
			;

		if (j == ALPHABET_SIZE) {
			pmemobj_tx_add_range(node->oid, 0,
					sizeof(*node) + D_RO(*node)->key_size);
			TX_FREE(*node);
			(*node) = TOID_NULL(struct tree_map_node);
			/* XXX check for parent nodes */
		}

		return ret;
	}

	/* Recurse deeply */
	return rtree_map_remove_node(map,
			&D_RW(*node)->slots[key[i]],
			key + i, key_size - i);
}

/*
 * rtree_map_remove -- removes key-value pair from the map
 */
PMEMoid
rtree_map_remove(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size)
{
	PMEMoid ret = OID_NULL;

	if (TOID_IS_NULL(map))
		return OID_NULL;

	TX_BEGIN(pop) {
		ret = rtree_map_remove_node(map,
				&D_RW(map)->root, key, key_size);
	} TX_END

	return ret;
}

/*
 * rtree_map_remove_free -- removes and frees an object from the tree
 */
int
rtree_map_remove_free(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size)
{
	int ret = 0;

	if (TOID_IS_NULL(map))
		return 1;

	TX_BEGIN(pop) {
		pmemobj_tx_free(rtree_map_remove(pop, map, key, key_size));
	} TX_ONABORT {
		ret = 1;
	} TX_END

	return ret;
}

/*
 * rtree_map_get_in_node -- (internal) searches for a value in the node
 */
static PMEMoid
rtree_map_get_in_node(TOID(struct tree_map_node) node,
		const unsigned char *key, uint64_t key_size)
{
	unsigned i;

	if (TOID_IS_NULL(node))
		return OID_NULL;

	i = key_comm_len(node, key, key_size);

	if (i != D_RO(node)->key_size)
		/* Node does not exist */
		return OID_NULL;

	if (i == key_size) {
		/* Node is found */
		return D_RO(node)->value;
	} else {
		/* Recurse deeply */
		return rtree_map_get_in_node(D_RO(node)->slots[key[i]],
				key + i, key_size - i);
	}
}

/*
 * rtree_map_get -- searches for a value of the key
 */
PMEMoid
rtree_map_get(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size)
{
	if (TOID_IS_NULL(D_RO(map)->root))
		return OID_NULL;

	return rtree_map_get_in_node(D_RO(map)->root, key, key_size);
}

/*
 * rtree_map_lookup_in_node -- (internal) searches for key if exists
 */
static int
rtree_map_lookup_in_node(TOID(struct tree_map_node) node,
		const unsigned char *key, uint64_t key_size)
{
	unsigned i;

	if (TOID_IS_NULL(node))
		return 0;

	i = key_comm_len(node, key, key_size);

	if (i != D_RO(node)->key_size)
		/* Node does not exist */
		return 0;

	if (i == key_size) {
		/* Node is found */
		return 1;
	}

	/* Recurse deeply */
	return rtree_map_lookup_in_node(D_RO(node)->slots[key[i]],
			key + i, key_size - i);
}

/*
 * rtree_map_lookup -- searches if key exists
 */
int
rtree_map_lookup(PMEMobjpool *pop, TOID(struct rtree_map) map,
		const unsigned char *key, uint64_t key_size)
{
	if (TOID_IS_NULL(D_RO(map)->root))
		return 0;

	return rtree_map_lookup_in_node(D_RO(map)->root, key, key_size);
}

/*
 * rtree_map_foreach_node -- (internal) recursively traverses tree
 */
static int
rtree_map_foreach_node(const TOID(struct tree_map_node) node,
	int (*cb)(const unsigned char *key, uint64_t key_size,
			PMEMoid, void *arg),
	void *arg)
{
	unsigned i;

	if (TOID_IS_NULL(node))
		return 0;

	for (i = 0; i < ALPHABET_SIZE; i++) {
		if (rtree_map_foreach_node(D_RO(node)->slots[i], cb, arg) != 0)
			return 1;
	}

	if (NULL != cb) {
		if (cb(D_RO(node)->key, D_RO(node)->key_size,
					D_RO(node)->value, arg) != 0)
			return 1;
	}

	return 0;
}

/*
 * rtree_map_foreach -- initiates recursive traversal
 */
int
rtree_map_foreach(PMEMobjpool *pop, TOID(struct rtree_map) map,
	int (*cb)(const unsigned char *key, uint64_t key_size,
			PMEMoid value, void *arg),
	void *arg)
{
	return rtree_map_foreach_node(D_RO(map)->root, cb, arg);
}

/*
 * ctree_map_check -- check if given persistent object is a tree map
 */
int
rtree_map_check(PMEMobjpool *pop, TOID(struct rtree_map) map)
{
	return TOID_IS_NULL(map) || !TOID_VALID(map);
}
