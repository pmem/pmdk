// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2022, Intel Corporation */

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
static void write_fields(struct fields *fieldsp, struct markers sm)
{
	VALGRIND_EMIT_LOG(sm.markers[0]);

	VALGRIND_EMIT_LOG(sm.markers[1]);

	fieldsp->a = 1;
	fieldsp->b = 1;
	fieldsp->c = 1;
	fieldsp->d = 1;
	pmem_persist(&fieldsp->a, sizeof(int) * 4);

	VALGRIND_EMIT_LOG(sm.markers[2]);

	fieldsp->e = 1;
	fieldsp->f = 1;
	fieldsp->g = 1;
	fieldsp->h = 1;
	pmem_persist(&fieldsp->e, sizeof(int) * 4);

	VALGRIND_EMIT_LOG(sm.markers[3]);

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
check_consistency(struct fields *fieldsp, struct markers sm)
{
	int consistency = 1;
	if (fieldsp->e == 1 && fieldsp->f == 0)
		consistency = 0;

	struct markers *log = get_markers(os_getenv("PMREORDER_MARKERS"));
	if (log) {
		if (log->markers_no != sm.markers_no)
			consistency = 1;
		else {
			for (size_t i = 0; i < (int)log->markers_no; i++)
				consistency &=
					strcmp(log->markers[i], sm.markers[i]);
		}
	}
	switch (log->markers_no) {
		case 2:
			consistency &=
				(fieldsp->a == 1 && fieldsp->b == 1 &&
				fieldsp->c == 1 && fieldsp->d == 1);
			break;
		case 3:
			consistency &=
				(fieldsp->e == 1 && fieldsp->f == 1 &&
				fieldsp->g == 1 && fieldsp->h == 1);
			break;
		case 4:
			consistency &=
				(fieldsp->i == 1 && fieldsp->j == 1 &&
				fieldsp->k == 1 && fieldsp->l == 1);
			break;
		default:
			break;
	}

	delete_markers(log);
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

	char *logs[] = {"FIELDS_PACK_TWO.BEGIN", "FIELDS_PACK_ONE.BEGIN",
		"FIELDS_PACK_ONE.END", "FIELDS_PACK_TWO.END"};

	struct markers stack_markers;
	stack_markers.markers_no = 4;
	stack_markers.markers = logs;

	/* clear the struct to get a consistent start state for writing */
	if (strchr("w", opt))
		pmem_memset_persist(fieldsp, 0, sizeof(*fieldsp));

	switch (opt) {
		case 'w':
			write_fields(fieldsp, stack_markers);
			break;
		case 'c':
			return check_consistency(fieldsp, stack_markers);
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}

	CLOSE(fd);

	DONE(NULL);
}
