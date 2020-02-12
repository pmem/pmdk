// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * main.c -- example usage of a slab-like mechanism implemented in libpmemobj
 *
 * This application does nothing besides demonstrating the example slab
 * allocator mechanism.
 *
 * By using the CTL alloc class API we can instrument libpmemobj to optimally
 * manage memory for the pool.
 */

#include <ex_common.h>
#include <assert.h>
#include <stdio.h>
#include "slab_allocator.h"

POBJ_LAYOUT_BEGIN(slab_allocator);
POBJ_LAYOUT_ROOT(slab_allocator, struct root);
POBJ_LAYOUT_TOID(slab_allocator, struct bar);
POBJ_LAYOUT_TOID(slab_allocator, struct foo);
POBJ_LAYOUT_END(slab_allocator);

struct foo {
	char data[100];
};

struct bar {
	char data[500];
};

struct root {
	TOID(struct foo) foop;
	TOID(struct bar) barp;
};

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("usage: %s file-name\n", argv[0]);
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

	struct slab_allocator *foo_producer = slab_new(pop, sizeof(struct foo));
	assert(foo_producer != NULL);
	struct slab_allocator *bar_producer = slab_new(pop, sizeof(struct bar));
	assert(bar_producer != NULL);

	TOID(struct root) root = POBJ_ROOT(pop, struct root);

	if (TOID_IS_NULL(D_RO(root)->foop)) {
		TX_BEGIN(pop) {
			TX_SET(root, foop.oid, slab_tx_alloc(foo_producer));
		} TX_END
	}

	if (TOID_IS_NULL(D_RO(root)->barp)) {
		slab_alloc(bar_producer, &D_RW(root)->barp.oid, NULL, NULL);
	}

	assert(pmemobj_alloc_usable_size(D_RO(root)->foop.oid) ==
							sizeof(struct foo));

	assert(pmemobj_alloc_usable_size(D_RO(root)->barp.oid) ==
							sizeof(struct bar));

	slab_delete(foo_producer);
	slab_delete(bar_producer);
	pmemobj_close(pop);

	return 0;
}
