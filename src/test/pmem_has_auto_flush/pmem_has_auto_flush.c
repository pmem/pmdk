// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * pmem_has_auto_flush.c -- unit test for pmem_has_auto_flush() function
 *
 * this test checks if function pmem_has_auto_flush handle sysfs path
 * and persistence_domain file in proper way
 */

#include <string.h>
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_has_auto_flush");

	if (argc != 1)
		UT_FATAL("usage: %s path", argv[0]);

	int ret = pmem_has_auto_flush();

	UT_OUT("pmem_has_auto_flush %d", ret);

	DONE(NULL);
}
