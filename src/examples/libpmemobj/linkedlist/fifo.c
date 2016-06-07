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
 * fifo.c - example of tail queue usage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pmemobj_list.h"

POBJ_LAYOUT_BEGIN(list);
POBJ_LAYOUT_ROOT(list, struct fifo_root);
POBJ_LAYOUT_TOID(list, struct tqnode);
POBJ_LAYOUT_END(list);

struct fifo_root {
	POBJ_TAILQ_HEAD(tqueuehead, struct tqnode) head;
};

struct tqnode {
	char data;
	POBJ_TAILQ_ENTRY(struct tqnode) tnd;
};

static void
print_help()
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

	if (access(path, F_OK) != 0) {
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
