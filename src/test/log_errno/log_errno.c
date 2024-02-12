// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * log_errno.c -- unit test for CORE_LOG_ERROR_W_ERRNO macro
 */
#include <syslog.h>

#include "unittest.h"
#include "log_internal.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "log_errno");

	core_log_init();
	CORE_LOG_ERROR_W_ERRNO("open file %s", "lolek");
	core_log_fini();
	/*
	 * The fini function above intentionally does not close the syslog
	 * socket. It has to be closed separately so it won't be accounted as
	 * an unclosed file descriptor.
	 */
	closelog();

	DONE(NULL);
}
