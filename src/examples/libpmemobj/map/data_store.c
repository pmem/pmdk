// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * data_store.c -- tree_map example usage
 */

#include <ex_common.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "map.h"
#include "map_ctree.h"
#include "map_btree.h"
#include "map_rbtree.h"
#include "map_hashmap_atomic.h"
#include "map_hashmap_tx.h"
#include "map_hashmap_rp.h"
#include "map_skiplist.h"

POBJ_LAYOUT_BEGIN(data_store);
POBJ_LAYOUT_ROOT(data_store, struct store_root);
POBJ_LAYOUT_TOID(data_store, struct store_item);
POBJ_LAYOUT_END(data_store);

#define MAX_INSERTS 500

static uint64_t nkeys;
static uint64_t keys[MAX_INSERTS];

struct store_item {
	uint64_t item_data;
};

struct store_root {
	TOID(struct map) map;
};

typedef int (*insert_rand_fn)(struct map_ctx *mapc,
		TOID(struct store_root) *root, int nops);

/*
 * new_store_item_transact -- transactionally creates and initializes new item
 */
static TOID(struct store_item)
new_store_item_transact(void)
{
	TOID(struct store_item) item = TX_NEW(struct store_item);
	D_RW(item)->item_data = rand();

	return item;
}

/*
 * store_item_construct -- initializes data of the new store item
 */
static int
store_item_construct(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct store_item *item = (struct store_item *)ptr;
	item->item_data = rand();
	pmemobj_persist(pop, item, sizeof(*item));

	return 0;
}

/*
 * new_store_item -- creates and initializes new item
 */
static TOID(struct store_item)
new_store_item(PMEMobjpool *pop)
{
	TOID(struct store_item) item;
	POBJ_NEW(pop, &item, struct store_item, store_item_construct, NULL);

	return item;
}

/*
 * insert_rand_items -- insert new random items
 */
static int insert_rand_items(struct map_ctx *mapc,
		TOID(struct store_root) *root, int nops)
{
	map_create(mapc, &D_RW(*root)->map, NULL);

	/* insert random items */
	for (int i = 0; i < nops; ++i)
		map_insert(mapc, D_RW(*root)->map, rand(),
				new_store_item(mapc->pop).oid);

	return 0;
}

/*
 * insert_rand_items_transact -- insert new random items transactionally
 */
static int insert_rand_items_transact(struct map_ctx *mapc,
		TOID(struct store_root) *root, int nops)
{
	int aborted = 0;

	/* transactional APIs should be inside transaction block */
	TX_BEGIN(mapc->pop) {
		map_create(mapc, &D_RW(*root)->map, NULL);

		/* insert random items in transaction */
		for (int i = 0; i < nops; ++i)
			map_insert(mapc, D_RW(*root)->map, rand(),
					new_store_item_transact().oid);
	} TX_ONABORT {
		perror("transaction aborted\n");
		map_ctx_free(mapc);
		aborted = 1;
	} TX_END

	return aborted;
}

/*
 * get_keys -- inserts the keys of the items by key order (sorted, descending)
 */
static int
get_keys(uint64_t key, PMEMoid value, void *arg)
{
	keys[nkeys++] = key;

	return 0;
}

/*
 * dec_keys -- decrements the keys count for every item
 */
static int
dec_keys(uint64_t key, PMEMoid value, void *arg)
{
	nkeys--;
	return 0;
}

/*
 * parse_map_type -- parse type of map
 */
static const struct map_ops *
parse_map_type(const char *type, insert_rand_fn *insert)
{
	if (strcmp(type, "ctree") == 0) {
		*insert = insert_rand_items_transact;
		return MAP_CTREE;
	} else if (strcmp(type, "btree") == 0) {
		*insert = insert_rand_items_transact;
		return MAP_BTREE;
	} else if (strcmp(type, "rbtree") == 0) {
		*insert = insert_rand_items_transact;
		return MAP_RBTREE;
	} else if (strcmp(type, "hashmap_atomic") == 0) {
		*insert = insert_rand_items;
		return MAP_HASHMAP_ATOMIC;
	} else if (strcmp(type, "hashmap_tx") == 0) {
		*insert = insert_rand_items_transact;
		return MAP_HASHMAP_TX;
	} else if (strcmp(type, "hashmap_rp") == 0) {
		*insert = insert_rand_items;
		return MAP_HASHMAP_RP;
	} else if (strcmp(type, "skiplist") == 0) {
		*insert = insert_rand_items_transact;
		return MAP_SKIPLIST;
	}
	return NULL;

}

int main(int argc, const char *argv[]) {
	if (argc < 3) {
		printf("usage: %s "
			"<ctree|btree|rbtree|hashmap_atomic|hashmap_rp|"
			"hashmap_tx|skiplist> file-name [nops]\n", argv[0]);
		return 1;
	}

	const char *type = argv[1];
	const char *path = argv[2];

	insert_rand_fn insert_items;
	const struct map_ops *map_ops = parse_map_type(type, &insert_items);
	if (!map_ops) {
		fprintf(stderr, "invalid container type -- '%s'\n", type);
		return 1;
	}

	int nops = MAX_INSERTS;

	if (argc > 3) {
		nops = atoi(argv[3]);
		if (nops <= 0 || nops > MAX_INSERTS) {
			fprintf(stderr, "number of operations must be "
				"in range 1..%d\n", MAX_INSERTS);
			return 1;
		}
	}

	PMEMobjpool *pop;
	srand((unsigned)time(NULL));

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(data_store),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(data_store))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

	TOID(struct store_root) root = POBJ_ROOT(pop, struct store_root);

	struct map_ctx *mapc = map_ctx_init(map_ops, pop);
	if (!mapc) {
		perror("cannot allocate map context\n");
		return 1;
	}
	/* delete the map if it exists */
	if (!map_check(mapc, D_RW(root)->map))
		map_destroy(mapc, &D_RW(root)->map);

	/* insert random items */
	if (insert_items(mapc, &root, nops))
		return 1;

	/* count the items */
	map_foreach(mapc, D_RW(root)->map, get_keys, NULL);

	/* remove the items without outer transaction */
	for (uint64_t i = 0; i < nkeys; ++i) {
		PMEMoid item = map_remove(mapc, D_RW(root)->map, keys[i]);

		assert(!OID_IS_NULL(item));
		assert(OID_INSTANCEOF(item, struct store_item));
	}

	uint64_t old_nkeys = nkeys;

	/* tree should be empty */
	map_foreach(mapc, D_RW(root)->map, dec_keys, NULL);
	assert(old_nkeys == nkeys);

	map_ctx_free(mapc);
	pmemobj_close(pop);

	return 0;
}
