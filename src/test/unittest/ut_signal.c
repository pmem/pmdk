// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * ut_signal.c -- unit test signal operations
 */

#include "unittest.h"

/*
 * ut_sigaction -- a sigaction that cannot return < 0
 */
int
ut_sigaction(const char *file, int line, const char *func,
	int signum, struct sigaction *act, struct sigaction *oldact)
{
	int retval = sigaction(signum, act, oldact);
	if (retval != 0)
		ut_fatal(file, line, func, "!sigaction: %s",
			os_strsignal(signum));
	return retval;
}
