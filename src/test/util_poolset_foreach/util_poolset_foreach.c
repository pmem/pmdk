// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * util_poolset_foreach.c -- unit test for util_poolset_foreach_part()
 *
 * usage: util_poolset_foreach file...
 */

#include "unittest.h"
#include "set.h"
#include "pmemcommon.h"
#include <errno.h>

#define LOG_PREFIX "ut"
#define LOG_LEVEL_VAR "TEST_LOG_LEVEL"
#define LOG_FILE_VAR "TEST_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

static int
cb(struct part_file *pf, void *arg)
{
	if (pf->is_remote) {
		/* remote replica */
		const char *node_addr = pf->remote->node_addr;
		const char *pool_desc = pf->remote->pool_desc;
		char *set_name = (char *)arg;
		UT_OUT("%s: %s %s", set_name, node_addr, pool_desc);
	} else {
		const char *name = pf->part->path;
		char *set_name = (char *)arg;
		UT_OUT("%s: %s", set_name, name);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_poolset_foreach");

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	if (argc < 2)
		UT_FATAL("usage: %s file...",
			argv[0]);

	for (int i = 1; i < argc; i++) {
		char *fname = argv[i];
		int ret = util_poolset_foreach_part(fname, cb, fname);

		UT_OUT("util_poolset_foreach_part(%s): %d", fname, ret);
	}
	common_fini();

	DONE(NULL);
}
