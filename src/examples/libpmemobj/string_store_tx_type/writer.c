// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * writer.c -- example from introduction part 3
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

	PMEMobjpool *pop = pmemobj_create(argv[1],
			POBJ_LAYOUT_NAME(string_store), PMEMOBJ_MIN_POOL, 0666);

	if (pop == NULL) {
		perror("pmemobj_create");
		return 1;
	}

	char buf[MAX_BUF_LEN] = {0};
	int num = scanf("%9s", buf);

	if (num == EOF) {
		fprintf(stderr, "EOF\n");
		return 1;
	}

	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	TX_BEGIN(pop) {
		TX_MEMCPY(D_RW(root)->buf, buf, strlen(buf));
	} TX_END

	pmemobj_close(pop);

	return 0;
}
