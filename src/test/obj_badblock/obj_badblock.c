// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * obj_badblock.c -- Badblock tests on obj pool
 *
 */

#include <stddef.h>

#include "unittest.h"
#include "libpmemobj.h"

#define LAYOUT_NAME "obj_badblock"
#define TEST_EXTEND_COUNT 32
#define EXTEND_SIZE (1024 * 1024 * 10)

static void
do_create_and_extend(const char *path)
{
	PMEMobjpool *pop = NULL;
	if ((pop = pmemobj_create(path, LAYOUT_NAME,
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	ssize_t extend_size = EXTEND_SIZE;
	for (int i = 0; i < TEST_EXTEND_COUNT; ++i) {
		int ret = pmemobj_ctl_exec(pop, "heap.size.extend",
			&extend_size);
		UT_ASSERTeq(ret, 0);
	}

	pmemobj_close(pop);
	UT_ASSERT(pmemobj_check(path, LAYOUT_NAME) == 1);
}

static void
do_open(const char *path)
{
	PMEMobjpool *pop = pmemobj_open(path, LAYOUT_NAME);
	UT_ASSERT(pop != NULL);
	pmemobj_close(pop);
}

int main(int argc, char **argv) {
	START(argc, argv, "obj_badblock");

	if (argc < 3)
		UT_FATAL("usage: %s file-name, o|c", argv[0]);

	const char *path = argv[1];

	for (int arg = 2; arg < argc; arg++) {
		if (argv[arg][1] != '\0')
			UT_FATAL(
				"op must be c or o (c=create, o=open)");
		switch (argv[arg][0]) {
		case 'c':
			do_create_and_extend(path);
			break;
		case 'o':
			do_open(path);
		default:
			UT_FATAL(
				"op must be c or o (c=clear, o=open)");
			break;
		}
	}

	DONE(NULL);
}
