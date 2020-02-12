// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * obj_ctl_config.c -- tests for ctl configuration
 */

#include "unittest.h"
#include "out.h"

#define LAYOUT "obj_ctl_config"

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_config");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];
	PMEMobjpool *pop = pmemobj_open(path, LAYOUT);
	if (pop == NULL)
		UT_FATAL("!pmemobj_open: %s", path);

	/* dump all available ctl read entry points */
	int result;
	pmemobj_ctl_get(pop, "prefault.at_open", &result);
	UT_OUT("%d", result);
	pmemobj_ctl_get(pop, "prefault.at_create", &result);
	UT_OUT("%d", result);

	pmemobj_close(pop);

	DONE(NULL);
}
