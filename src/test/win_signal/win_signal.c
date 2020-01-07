// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * win_signal.c -- test signal related routines
 */

#include "unittest.h"

extern int sys_siglist_size;

int
main(int argc, char *argv[])
{
	int sig;

	START(argc, argv, "win_signal");
	for (sig = 0; sig < sys_siglist_size; sig++) {
		UT_OUT("%d; %s", sig, os_strsignal(sig));
	}
	for (sig = 33; sig < 66; sig++) {
		UT_OUT("%d; %s", sig, os_strsignal(sig));
	}
	DONE(NULL);
}
