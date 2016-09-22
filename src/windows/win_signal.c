/*
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * win_signal.c -- Windows emulation of Linux specific APIs
 */

/*
 * sys_siglist -- map of signal to human readable messages like sys_siglist
 */
const char * const sys_siglist[] = {
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
int sys_siglist_size = ARRAYSIZE(sys_siglist);

/*
 * string constants for strsignal
 * XXX: ideally this should have the signal number as the suffix but then we
 * should use a buffer from thread local storage, so deferring the same till
 * we need it
 * NOTE: In Linux strsignal uses TLS for the same reason but if it fails to get
 * a thread local buffer it falls back to using a static buffer trading the
 * thread safety.
 */
#define STR_REALTIME_SIGNAL	"Real-time signal"
#define STR_UNKNOWN_SIGNAL	"Unknown signal"

/*
 * strsignal -- returns a string describing the signal number 'sig'
 *
 * XXX: According to POSIX, this one is of type 'char *', but in our
 * implementation it returns 'const char *'.
 */
const char *
strsignal(int sig)
{
	if (sig >= 0 && sig < ARRAYSIZE(sys_siglist))
		return sys_siglist[sig];
	else if (sig >= 34 && sig <= 64)
		return STR_REALTIME_SIGNAL;
	else
		return STR_UNKNOWN_SIGNAL;
}
