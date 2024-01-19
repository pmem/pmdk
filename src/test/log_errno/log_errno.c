// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * log_errno.c -- unit test for CORE_LOG_ERROR_WITH_ERRNO macro
 */


#include "unittest.h"
#include "log_internal.h"

int
main(int argc, char *argv[])
{
	core_log_init();
	START(argc, argv, "log_errno");

	CORE_LOG_ERROR_WITH_ERRNO("open file %s", "lolek");
	DONE(NULL);
}
