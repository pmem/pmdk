// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * btree.c -- implementation of persistent binary search tree
 */

#include <ex_common.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(btree);
POBJ_LAYOUT_ROOT(btree, struct btree);
POBJ_LAYOUT_TOID(btree, struct btree_node);
POBJ_LAYOUT_END(btree);

struct btree_node {
	int64_t key;
	TOID(struct btree_node) slots[2];
	char value[];
};

struct btree {
	TOID(struct btree_node) root;
};

struct btree_node_arg {
	size_t size;
	int64_t key;
	const char *value;
};

/*
 * btree_node_construct -- constructor of btree node
 */
static int
btree_node_construct(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct btree_node *node = (struct btree_node *)ptr;
	struct btree_node_arg *a = (struct btree_node_arg *)arg;

	node->key = a->key;
	strcpy(node->value, a->value);
	node->slots[0] = TOID_NULL(struct btree_node);
	node->slots[1] = TOID_NULL(struct btree_node);

	pmemobj_persist(pop, node, a->size);

	return 0;
}

/*
 * btree_insert -- inserts new element into the tree
 */
static void
btree_insert(PMEMobjpool *pop, int64_t key, const char *value)
{
	TOID(struct btree) btree = POBJ_ROOT(pop, struct btree);
	TOID(struct btree_node) *dst = &D_RW(btree)->root;

	while (!TOID_IS_NULL(*dst)) {
		dst = &D_RW(*dst)->slots[key > D_RO(*dst)->key];
	}

	struct btree_node_arg args;
	args.size = sizeof(struct btree_node) + strlen(value) + 1;
	args.key = key;
	args.value = value;

	POBJ_ALLOC(pop, dst, struct btree_node, args.size,
		btree_node_construct, &args);
}

/*
 * btree_find -- searches for key in the tree
 */
static const char *
btree_find(PMEMobjpool *pop, int64_t key)
{
	TOID(struct btree) btree = POBJ_ROOT(pop, struct btree);
	TOID(struct btree_node) node = D_RO(btree)->root;

	while (!TOID_IS_NULL(node)) {
		if (D_RO(node)->key == key)
			return D_RO(node)->value;
		else
			node = D_RO(node)->slots[key > D_RO(node)->key];
	}

	return NULL;
}

/*
 * btree_node_print -- prints content of the btree node
 */
static void
btree_node_print(const TOID(struct btree_node) node)
{
	printf("%" PRIu64 " %s\n", D_RO(node)->key, D_RO(node)->value);
}

/*
 * btree_foreach -- invoke callback for every node
 */
static void
btree_foreach(PMEMobjpool *pop, const TOID(struct btree_node) node,
	void(*cb)(const TOID(struct btree_node) node))
{
	if (TOID_IS_NULL(node))
		return;

	btree_foreach(pop, D_RO(node)->slots[0], cb);

	cb(node);

	btree_foreach(pop, D_RO(node)->slots[1], cb);
}

/*
 * btree_print -- initiates foreach node print
 */
static void
btree_print(PMEMobjpool *pop)
{
	TOID(struct btree) btree = POBJ_ROOT(pop, struct btree);

	btree_foreach(pop, D_RO(btree)->root, btree_node_print);
}

int
main(int argc, char *argv[])
{
	if (argc < 3) {
		printf(
			"usage: %s file-name [p|i|f] [key (int64_t != 0)] [value (str)]\n",
			argv[0]);
		return 1;
	}

	const char *path = argv[1];

	PMEMobjpool *pop;

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(btree),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(btree))) == NULL) {
			perror("failed to open pool\n");
			return 1;
		}
	}

	const char op = argv[2][0];
	int64_t key;
	const char *value;

	switch (op) {
		case 'p':
			btree_print(pop);
		break;
		case 'i':
			key = atoll(argv[3]);
			/*
			 * atoll returns 0 if conversion failed;
			 * disallow 0 as a key
			 */
			if (key == 0) {
				printf(
					"Expecting a key of type int64, not equal to 0\n");
			} else {
				value = argv[4];
				btree_insert(pop, key, value);
				printf(
					"Inserted key: '%ld' with value: '%s'\n",
					(long)key, value);
			}
		break;
		case 'f':
			key = atoll(argv[3]);
			if ((value = btree_find(pop, key)) != NULL)
				printf("%s\n", value);
			else
				printf("not found\n");
		break;
		default:
			printf("invalid operation\n");
		break;
	}

	pmemobj_close(pop);

	return 0;
}
