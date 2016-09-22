/*
 * Copyright 2014-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * win_signal.c -- test signal related routines
 */

#include "unittest.h"
#include "win_signal.h"

const char * const test_siglist[] = {
	"Unknown signal 0",		/*  0 */
	"Hangup",			/*  1 */
	"Interrupt",			/*  2 */
	"Quit",				/*  3 */
	"Illegal instruction",		/*  4 */
	"Trace/breakpoint trap",	/*  5 */
	"Aborted",			/*  6 */
	"Bus error",			/*  7 */
	"Floating point exception",	/*  8 */
	"Killed",			/*  9 */
	"User defined signal 1",	/* 10 */
	"Segmentation fault",		/* 11 */
	"User defined signal 2",	/* 12 */
	"Broken pipe",			/* 13 */
	"Alarm clock",			/* 14 */
	"Terminated",			/* 15 */
	"Stack fault",			/* 16 */
	"Child exited",			/* 17 */
	"Continued",			/* 18 */
	"Stopped (signal)",		/* 19 */
	"Stopped",			/* 20 */
	"Stopped (tty input)",		/* 21 */
	"Stopped (tty output)",		/* 22 */
	"Urgent I/O condition",		/* 23 */
	"CPU time limit exceeded",	/* 24 */
	"File size limit exceeded",	/* 25 */
	"Virtual timer expired",	/* 26 */
	"Profiling timer expired",	/* 27 */
	"Window changed",		/* 28 */
	"I/O possible",			/* 29 */
	"Power failure",		/* 30 */
	"Bad system call",		/* 31 */
	"Unknown signal 32"		/* 32 */
};

int
main(int argc, char *argv[])
{
	int sig;

	START(argc, argv, "win_signal");

	UT_ASSERTeq_rt(ARRAYSIZE(sys_siglist), ARRAYSIZE(test_siglist));
	for (sig = 0; sig < ARRAYSIZE(sys_siglist); sig++) {
		UT_ASSERTeq_rt(strsignal(sig), test_siglist[sig]);
	}
	for (sig = 34; sig < 65; sig++) {
		UT_ASSERTeq_rt(strsignal(sig), STR_REALTIME_SIGNAL);
	}
	UT_ASSERTeq_rt(strsignal(33), STR_UNKNOWN_SIGNAL);
	UT_ASSERTeq_rt(strsignal(65), STR_UNKNOWN_SIGNAL);

	DONE(NULL);
}
