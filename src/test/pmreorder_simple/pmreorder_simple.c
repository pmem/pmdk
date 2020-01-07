// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * pmreorder_simple.c -- a simple unit test for store reordering
 *
 * usage: pmreorder_simple g|b|c|m file
 * g - write data in a consistent manner
 * b - write data in a possibly inconsistent manner
 * c - check data consistency
 * m - write data to the pool in a consistent way,
 * but at the beginning logs some inconsistent values
 *
 * See README file for more details.
 */

#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

/*
 * The struct three_field is inconsistent if flag is set and the fields have
 * different values.
 */
struct three_field {
	int first_field;
	int second_field;
	int third_field;
	int flag;
};

/*
 * write_consistent -- (internal) write data in a consistent manner
 */
static void
write_consistent(struct three_field *structp)
{
	structp->first_field = 1;
	structp->second_field = 1;
	structp->third_field = 1;
	pmem_persist(&structp->first_field, sizeof(int) * 3);
	structp->flag = 1;
	pmem_persist(&structp->flag, sizeof(structp->flag));
}

/*
 * write_inconsistent -- (internal) write data in an inconsistent manner.
 */
static void
write_inconsistent(struct three_field *structp)
{
	structp->flag = 1;
	structp->first_field = 1;
	structp->second_field = 1;
	structp->third_field = 1;
	pmem_persist(structp, sizeof(*structp));
}

/*
 * check_consistency -- (internal) check struct three_field consistency
 */
static int
check_consistency(struct three_field *structp)
{
	int consistent = 0;
	if (structp->flag)
		consistent = (structp->first_field != structp->second_field) ||
			(structp->first_field != structp->third_field);
	return consistent;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmreorder_simple");

	util_init();

	if ((argc != 3) || (strchr("gbcm", argv[1][0]) == NULL) ||
			argv[1][1] != '\0')
		UT_FATAL("usage: %s g|b|c|m file", argv[0]);

	int fd = OPEN(argv[2], O_RDWR);
	size_t size;
	/* mmap and register in valgrind pmemcheck */
	void *map = pmem_map_file(argv[2], 0, 0, 0, &size, NULL);
	UT_ASSERTne(map, NULL);

	struct three_field *structp = map;

	char opt = argv[1][0];

	/* clear the struct to get a consistent start state for writing */
	if (strchr("gb", opt))
		pmem_memset_persist(structp, 0, sizeof(*structp));
	else if (strchr("m", opt)) {
		/* set test values to log an inconsistent start state */
		pmem_memset_persist(&structp->flag, 1, sizeof(int));
		pmem_memset_persist(&structp->first_field, 0, sizeof(int) * 2);
		pmem_memset_persist(&structp->third_field, 1, sizeof(int));
		/* clear the struct to get back a consistent start state */
		pmem_memset_persist(structp, 0, sizeof(*structp));
	}

	/* verify that DEFAULT_REORDER restores default engine */
	VALGRIND_EMIT_LOG("PMREORDER_MARKER_CHANGE.BEGIN");

	switch (opt) {
		case 'g':
			write_consistent(structp);
			break;
		case 'b':
			write_inconsistent(structp);
			break;
		case 'm':
			write_consistent(structp);
			break;
		case 'c':
			return check_consistency(structp);
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}
	VALGRIND_EMIT_LOG("PMREORDER_MARKER_CHANGE.END");

	/* check if undefined marker will not cause an issue */
	VALGRIND_EMIT_LOG("PMREORDER_MARKER_UNDEFINED.BEGIN");
	VALGRIND_EMIT_LOG("PMREORDER_MARKER_UNDEFINED.END");

	CLOSE(fd);

	DONE(NULL);
}
