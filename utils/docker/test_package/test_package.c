// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

#include <libpmemobj.h>
#include <stdio.h>
#include <sys/stat.h>

#define LAYOUT_NAME "test"

struct my_root {
	int foo;
};

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("usage: %s file-name\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];

	PMEMobjpool *pop = pmemobj_create(path, LAYOUT_NAME,
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);

	if (pop == NULL) {
		printf("failed to create pool\n");
		return 1;
	}

	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	struct my_root *rootp = pmemobj_direct(root);

	rootp->foo = 10;
	pmemobj_persist(pop, &rootp->foo, sizeof(rootp->foo));

	pmemobj_close(pop);

	return 0;
}
