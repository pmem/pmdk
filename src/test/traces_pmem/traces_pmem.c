// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * traces_pmem.c -- unit test traces for libraries pmem
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "traces_pmem");

	UT_ASSERT(!pmem_check_version(PMEM_MAJOR_VERSION,
				PMEM_MINOR_VERSION));
	UT_ASSERT(!pmemobj_check_version(PMEMOBJ_MAJOR_VERSION,
				PMEMOBJ_MINOR_VERSION));

	DONE(NULL);
}
