// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2019, Intel Corporation */

/*
 * pmreorder_stack.c -- unit test for engines pmreorder stack
 *
 * usage: pmreorder_stack w|c file
 * w - write data in a possibly inconsistent manner
 * c - check data consistency
 *
 */

#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

/*
 * Consistent only if field 'e' is set and field 'f' is not.
 */
struct fields {
	int a;
	int b;
	int c;
	int d;

	int e;
	int f;
	int g;
	int h;

	int i;
	int j;
	int k;
	int l;
};

/*
 * write_fields -- (internal) write data in a consistent manner.
 */
static void
write_fields(struct fields *fieldsp)
{
	VALGRIND_EMIT_LOG("FIELDS_PACK_TWO.BEGIN");

	VALGRIND_EMIT_LOG("FIELDS_PACK_ONE.BEGIN");

	fieldsp->a = 1;
	fieldsp->b = 1;
	fieldsp->c = 1;
	fieldsp->d = 1;
	pmem_persist(&fieldsp->a, sizeof(int) * 4);

	VALGRIND_EMIT_LOG("FIELDS_PACK_ONE.END");

	fieldsp->e = 1;
	fieldsp->f = 1;
	fieldsp->g = 1;
	fieldsp->h = 1;
	pmem_persist(&fieldsp->e, sizeof(int) * 4);

	VALGRIND_EMIT_LOG("FIELDS_PACK_TWO.END");

	fieldsp->i = 1;
	fieldsp->j = 1;
	fieldsp->k = 1;
	fieldsp->l = 1;
	pmem_persist(&fieldsp->i, sizeof(int) * 4);
}

/*
 * check_consistency -- (internal) check struct fields consistency.
 */
static int
check_consistency(struct fields *fieldsp)
{
	int consistency = 1;
	if (fieldsp->e == 1 && fieldsp->f == 0)
		consistency = 0;

	return consistency;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmreorder_stack");

	util_init();

	if ((argc != 3) || (strchr("wc", argv[1][0]) == NULL) ||
			argv[1][1] != '\0')
		UT_FATAL("usage: %s w|c file", argv[0]);

	int fd = OPEN(argv[2], O_RDWR);
	size_t size;

	/* mmap and register in valgrind pmemcheck */
	void *map = pmem_map_file(argv[2], 0, 0, 0, &size, NULL);
	UT_ASSERTne(map, NULL);
	UT_ASSERT(size >= sizeof(struct fields));

	struct fields *fieldsp = map;

	char opt = argv[1][0];

	/* clear the struct to get a consistent start state for writing */
	if (strchr("w", opt))
		pmem_memset_persist(fieldsp, 0, sizeof(*fieldsp));

	switch (opt) {
		case 'w':
			write_fields(fieldsp);
			break;
		case 'c':
			return check_consistency(fieldsp);
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}

	CLOSE(fd);

	DONE(NULL);
}
