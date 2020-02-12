// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * fifo.c - example of tail queue usage
 */

#include <ex_common.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmemobj_list.h"

POBJ_LAYOUT_BEGIN(list);
POBJ_LAYOUT_ROOT(list, struct fifo_root);
POBJ_LAYOUT_TOID(list, struct tqnode);
POBJ_LAYOUT_END(list);

POBJ_TAILQ_HEAD(tqueuehead, struct tqnode);

struct fifo_root {
	struct tqueuehead head;
};

struct tqnode {
	char data;
	POBJ_TAILQ_ENTRY(struct tqnode) tnd;
};

static void
print_help(void)
{
	printf("usage: fifo <pool> <option> [<type>]\n");
	printf("\tAvailable options:\n");
	printf("\tinsert, <character> Insert character into FIFO\n");
	printf("\tremove, Remove element from FIFO\n");
	printf("\tprint, Print all FIFO elements\n");
}

int
main(int argc, const char *argv[])
{
	PMEMobjpool *pop;
	const char *path;

	if (argc < 3) {
		print_help();
		return 0;
	}
	path = argv[1];

	if (file_exists(path) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(list),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			perror("failed to create pool\n");
			return -1;
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(list))) == NULL) {
			perror("failed to open pool\n");
			return -1;
		}
	}

	TOID(struct fifo_root) root = POBJ_ROOT(pop, struct fifo_root);
	struct tqueuehead *tqhead = &D_RW(root)->head;
	TOID(struct tqnode) node;

	if (strcmp(argv[2], "insert") == 0) {
		if (argc == 4) {
			TX_BEGIN(pop) {
				node = TX_NEW(struct tqnode);
				D_RW(node)->data = *argv[3];
				POBJ_TAILQ_INSERT_HEAD(tqhead, node, tnd);
			} TX_ONABORT {
				abort();
			} TX_END
			printf("Added %c to FIFO\n", *argv[3]);
		} else {
			print_help();
		}
	} else if (strcmp(argv[2], "remove") == 0) {
		if (POBJ_TAILQ_EMPTY(tqhead)) {
			printf("FIFO is empty\n");
		} else {
			node = POBJ_TAILQ_LAST(tqhead);
			TX_BEGIN(pop) {
				POBJ_TAILQ_REMOVE_FREE(tqhead, node, tnd);
			} TX_ONABORT {
				abort();
			} TX_END
			printf("Removed element from FIFO\n");
		}
	} else if (strcmp(argv[2], "print") == 0) {
		printf("Elements in FIFO:\n");
		POBJ_TAILQ_FOREACH(node, tqhead, tnd) {
			printf("%c\t", D_RO(node)->data);
		}
		printf("\n");
	} else {
		print_help();
	}
	pmemobj_close(pop);
	return 0;
}
