// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * signal_handle.c -- unit test for signal_handle
 *
 *
 * operations are: 's', 'a', 'a', 'i', 'v'
 * s: testing SIGSEGV with signal_handler_2
 * a: testing SIGABRT with signal_handler_1
 * a: testing second occurrence of SIGABRT with signal_handler_1
 * i: testing SIGILL with signal_handler_2
 * v: testing third occurrence of SIGABRT with other signal_handler_3
 *
 */

#include "unittest.h"

ut_jmp_buf_t Jmp;

static void
signal_handler_1(int sig)
{
	UT_OUT("\tsignal_handler_1: %s", os_strsignal(sig));
	ut_siglongjmp(Jmp);
}

static void
signal_handler_2(int sig)
{
	UT_OUT("\tsignal_handler_2: %s", os_strsignal(sig));
	ut_siglongjmp(Jmp);
}

static void
signal_handler_3(int sig)
{
	UT_OUT("\tsignal_handler_3: %s", os_strsignal(sig));
	ut_siglongjmp(Jmp);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "signal_handle");

	if (argc < 2)
		UT_FATAL("usage: %s op:s|a|a|i|v", argv[0]);

	struct sigaction v1, v2, v3;
	sigemptyset(&v1.sa_mask);
	v1.sa_flags = 0;
	v1.sa_handler = signal_handler_1;

	sigemptyset(&v2.sa_mask);
	v2.sa_flags = 0;
	v2.sa_handler = signal_handler_2;

	SIGACTION(SIGSEGV, &v2, NULL);
	SIGACTION(SIGABRT, &v1, NULL);
	SIGACTION(SIGABRT, &v2, NULL);
	SIGACTION(SIGABRT, &v1, NULL);
	SIGACTION(SIGILL, &v2, NULL);

	for (int arg = 1; arg < argc; arg++) {
		if (strchr("sabiv", argv[arg][0]) == NULL ||
				argv[arg][1] != '\0')
			UT_FATAL("op must be one of: s, a, a, i, v");

		switch (argv[arg][0]) {
		case 's':
			UT_OUT("Testing SIGSEGV...");
			if (!ut_sigsetjmp(Jmp)) {
				if (!raise(SIGSEGV)) {
					UT_OUT("\t SIGSEGV occurrence");
				} else {
					UT_OUT("\t Issue with SIGSEGV raise");
				}
			}
			break;

		case 'a':
			UT_OUT("Testing SIGABRT...");
			if (!ut_sigsetjmp(Jmp)) {
				if (!raise(SIGABRT)) {
					UT_OUT("\t SIGABRT occurrence");
				} else {
					UT_OUT("\t Issue with SIGABRT raise");
				}
			}
			break;

		case 'i':
			UT_OUT("Testing SIGILL...");
			if (!ut_sigsetjmp(Jmp)) {
				if (!raise(SIGILL)) {
					UT_OUT("\t SIGILL occurrence");
				} else {
					UT_OUT("\t Issue with SIGILL raise");
				}
			}
			break;

		case 'v':
			if (!ut_sigsetjmp(Jmp)) {

				sigemptyset(&v3.sa_mask);
				v3.sa_flags = 0;
				v3.sa_handler = signal_handler_3;

				UT_OUT("Testing SIGABRT...");
				SIGACTION(SIGABRT, &v3, NULL);

				if (!raise(SIGABRT)) {
					UT_OUT("\t SIGABRT occurrence");
				} else {
					UT_OUT("\t Issue with SIGABRT raise");
				}
			}
			break;
		}
	}

	DONE(NULL);
}
