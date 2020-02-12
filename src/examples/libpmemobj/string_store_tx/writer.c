// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * writer.c -- example from introduction part 2
 */

#include <stdio.h>
#include <string.h>
#include <libpmemobj.h>

#include "layout.h"

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("usage: %s file-name\n", argv[0]);
		return 1;
	}

	PMEMobjpool *pop = pmemobj_create(argv[1], LAYOUT_NAME,
				PMEMOBJ_MIN_POOL, 0666);

	if (pop == NULL) {
		perror("pmemobj_create");
		return 1;
	}

	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	struct my_root *rootp = pmemobj_direct(root);

	char buf[MAX_BUF_LEN] = {0};
	if (scanf("%9s", buf) == EOF) {
		fprintf(stderr, "EOF\n");
		return 1;
	}

	TX_BEGIN(pop) {
		pmemobj_tx_add_range(root, 0, sizeof(struct my_root));
		memcpy(rootp->buf, buf, strlen(buf));
	} TX_END

	pmemobj_close(pop);

	return 0;
}
