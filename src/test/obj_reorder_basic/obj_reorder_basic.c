// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * obj_reorder_basic.c -- a simple unit test for store reordering
 *
 * usage: obj_reorder_basic file w|c
 * w - write data
 * c - check data consistency
 *
 */

#include "unittest.h"
#include "util.h"
#include "valgrind_internal.h"

#define LAYOUT_NAME "intro_1"
#define MAX_BUF_LEN 10
#define BUF_VALUE 'a'

struct my_root {
	size_t len;
	char buf[MAX_BUF_LEN];
};

/*
 * write_consistent -- (internal) write data in a consistent manner
 */
static void
write_consistent(struct pmemobjpool *pop)
{
	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	struct my_root *rootp = pmemobj_direct(root);

	char buf[MAX_BUF_LEN];
	memset(buf, BUF_VALUE, sizeof(buf));
	buf[MAX_BUF_LEN - 1] = '\0';

	rootp->len = strlen(buf);
	pmemobj_persist(pop, &rootp->len, sizeof(rootp->len));

	pmemobj_memcpy_persist(pop, rootp->buf, buf, rootp->len);
}

/*
 * check_consistency -- (internal) check buf consistency
 */
static int
check_consistency(struct pmemobjpool *pop)
{

	PMEMoid root = pmemobj_root(pop, sizeof(struct my_root));
	struct my_root *rootp = pmemobj_direct(root);

	if (rootp->len == strlen(rootp->buf) && rootp->len != 0)
		for (int i = 0; i < MAX_BUF_LEN - 1; ++i)
			if (rootp->buf[i] != BUF_VALUE)
				return 1;

	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_reorder_basic");

	util_init();

	if (argc != 3 || strchr("wc", argv[1][0]) == 0 || argv[1][1] != '\0')
		UT_FATAL("usage: %s w|c file", argv[0]);

	char opt = argv[1][0];
	if (opt == 'c') {
		int y = 1;
		pmemobj_ctl_set(NULL, "copy_on_write.at_open", &y);
	}

	PMEMobjpool *pop = pmemobj_open(argv[2], LAYOUT_NAME);
	UT_ASSERT(pop != NULL);

	VALGRIND_EMIT_LOG("PMREORDER_MARKER_WRITE.BEGIN");
	switch (opt) {
		case 'w':
		{
			write_consistent(pop);
			break;
		}
		case 'c':
		{
			int ret = check_consistency(pop);
			pmemobj_close(pop);
			END(ret);
		}
		default:
			UT_FATAL("Unrecognized option %c", opt);
	}
	VALGRIND_EMIT_LOG("PMREORDER_MARKER_WRITE.END");

	pmemobj_close(pop);
	DONE(NULL);
}
